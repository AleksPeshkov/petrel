#ifndef SEARCH_ROOT_HPP
#define SEARCH_ROOT_HPP

#include "chrono.hpp"
#include "out.hpp"
#include "PositionFen.hpp"
#include "PvMoves.hpp"
#include "SearchThread.hpp"
#include "Score.hpp"
#include "SpinLock.hpp"
#include "Timer.hpp"
#include "Tt.hpp"

class UciGoLimit;
class SearchRoot;

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

    NodeControl count(const SearchRoot&);
    NodeControl refreshQuota(const SearchRoot&);

};

class SearchRoot {
public:
    PositionFen position; // root position between 'position' and 'go' commands
    Tt tt;
    PvMoves pvMoves;

private:
    mutable node_count_t lastInfoNodes = 0; // to avoid printing identical nps info lines in a row
    mutable SpinLock outLock;
    mutable bool isreadyWaiting = false;

    ostream& out; // output stream
    TimePoint fromSearchStart;

    NodeCounter nodeCounter;
    SearchThread searchThread;
    Timer timer;

public:
    SearchRoot (ostream& o) : out{o} {}

    bool isBusy() const { return !searchThread.isIdle(); }
    bool isStopped() const { return searchThread.isStopped(); }
    void stop() { searchThread.stop(); }

    void uciok() const;
    void isready() const;
    void readyok() const;
    void setHash(size_t);

    void infoPosition() const;

    void go(const UciGoLimit&);
    void goPerft(Ply depth, bool isDivide = false);

    void newGame();
    void newSearch();
    void newIteration();

    void bestmove() const;
    void infoNewPv(Ply, Score) const;
    void infoIterationEnd(Ply) const;

    void perft_depth(Ply, node_count_t) const;
    void perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t) const;
    void perft_finish() const;

    NodeControl countNode();

protected:
    SearchRoot (const SearchRoot&) = delete;
    SearchRoot& operator=(const SearchRoot&) = delete;
    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
};

#endif
