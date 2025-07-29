#ifndef SEARCH_ROOT_HPP
#define SEARCH_ROOT_HPP

#include "chrono.hpp"
#include "out.hpp"
#include "PositionFen.hpp"
#include "PvMoves.hpp"
#include "Score.hpp"
#include "SpinLock.hpp"
#include "ThreadRun.hpp"
#include "Timer.hpp"
#include "Tt.hpp"

class SearchRoot;

enum class ReturnStatus {
    Continue,
    Abort,
    BetaCutoff,
};

#define RETURN_IF_ABORT(visitor) { if (visitor == ReturnStatus::Abort) { return ReturnStatus::Abort; } } ((void)0)
#define BREAK_IF_ABORT(visitor) { if (visitor == ReturnStatus::Abort) { break; } } ((void)0)
#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; \
    if (status == ReturnStatus::Abort) { return ReturnStatus::Abort; } \
    if (status == ReturnStatus::BetaCutoff) { return ReturnStatus::BetaCutoff; }} ((void)0)

class HistoryMoves {
    typedef Side::arrayOf<PieceType::arrayOf< Square::arrayOf<Move> >> _t;
    _t v;
public:
    void clear() { std::memset(&v, 0, sizeof(v)); }
    const Move& operator() (Color c, PieceType ty, Square sq) const { return v[c][ty][sq]; }
    void set(Color c, PieceType ty, Square sq, const Move& move) { v[c][ty][sq] = move; }
};

class NodeCounter {
    node_count_t nodes = 0; // (0 <= nodes && nodes <= nodesLimit)
    node_count_t nodesLimit; // search limit

    typedef unsigned nodes_quota_t;
    enum : nodes_quota_t { QuotaLimit = 1000 }; // ~0.1 msec

    //number of remaining nodes before slow checking for search abort
    nodes_quota_t nodesQuota = 0; // (0 <= nodesQuota && nodesQuota <= QuotaLimit)

    constexpr void assertOk() const {
        assert (nodesQuota <= nodes && nodes <= nodesLimit);
        assert (/* 0 <= nodesQuota && */ nodesQuota < QuotaLimit);
    }

public:
    constexpr NodeCounter(node_count_t n = NodeCountMax) : nodesLimit{n} {}

    /// exact number of visited nodes
    constexpr operator node_count_t () const {
        assertOk();
        return nodes - nodesQuota;
    }

    constexpr bool isAborted() const {
        assertOk();
        assert (nodes - nodesQuota < nodesLimit || nodesQuota == 0);
        return nodes == nodesLimit;
    }

    ReturnStatus count(const SearchRoot&);
    ReturnStatus refreshQuota(const SearchRoot&);

};

template<class BasicLockable>
class OutputBuffer : public std::ostringstream {
    io::ostream& out;
    BasicLockable& lock;
    typedef std::lock_guard<decltype(lock)> Guard;
public:
    OutputBuffer (io::ostream& o, BasicLockable& l) : std::ostringstream{}, out(o), lock(l) {}
    ~OutputBuffer () { Guard g{lock}; out << str() << std::flush; }
};

class SearchRoot {
public:
    PositionFen position; // root position between 'position' and 'go' commands
    Tt tt;
    PvMoves pvMoves;
    HistoryMoves counterMove;

protected:
    mutable node_count_t lastInfoNodes = 0; // to avoid printing identical nps info lines in a row
    mutable SpinLock outLock;
    mutable std::atomic<bool> isreadyWaiting = false;

    ostream& out; // output stream

    NodeCounter nodeCounter;

    TimePoint fromSearchStart;
    Timer timer;

    ThreadRun searchThread;

public:
    SearchRoot (ostream& o) : out{o} {}

    bool isStopped() const { return searchThread.isStopped(); }
    void readyok() const;

    void newIteration();

    void bestmove() const;
    void infoNewPv(Ply, Score) const;
    void infoIterationEnd(Ply) const;

    void perft_depth(Ply, node_count_t) const;
    void perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t) const;
    void perft_finish() const;

    ReturnStatus countNode();

protected:
    SearchRoot (const SearchRoot&) = delete;
    SearchRoot& operator=(const SearchRoot&) = delete;
    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
};

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

#endif
