#ifndef UCI_HPP
#define UCI_HPP

#include <atomic>
#include <fstream>
#include <mutex>
#include "chrono.hpp"
#include "io.hpp"
#include "Index.hpp"
#include "Repetitions.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciPosition.hpp"
#include "UciMove.hpp"

enum deadline_t { HardDeadline, RootMoveDeadline, IterationDeadline };

class UciSearchLimits {
    friend class Uci; // for bench()

    constexpr static int QuotaLimit = 1000;

    mutable node_count_t nodes = 0; // (0 <= nodes && nodes <= nodesLimit)
    mutable node_count_t nodesLimit = NodeCountMax; // search limit

    //number of remaining nodes before slow checking for search stop
    mutable int nodesQuota = 0; // (0 <= nodesQuota && nodesQuota <= QuotaLimit)

    mutable std::atomic_bool stop_{false};
    mutable std::atomic_bool infinite{false};
    mutable std::atomic_bool ponder{false};

    TimePoint searchStartTime;

    TimeInterval deadline[3] = {
        TimeInterval::max(), // HardDeadline      : tested every QuotaLimit nodes
        TimeInterval::max(), // RootMoveDeadline  : tested after every root move search ends
        TimeInterval::max()  // IterationDeadline : tested when iteration ends
    };

    Side::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Side::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};
    TimeInterval movetime = 0ms;

    int movestogo = 0;
    int mate = 0;

    constexpr void assertNodesOk() const {
        assert (0 <= nodesQuota);
        assert (nodesQuota < QuotaLimit);
        //assert (0 <= nodes);
        assert (nodes <= nodesLimit);
        assert (static_cast<decltype(nodesLimit)>(nodesQuota) <= nodes);
    }

    constexpr void setNoDeadline() {
        deadline[HardDeadline]      = TimeInterval::max();
        deadline[RootMoveDeadline]  = TimeInterval::max();
        deadline[IterationDeadline] = TimeInterval::max();
    }

    constexpr TimeInterval average(Side si) const {
        assert (movestogo >= 0);

        if (!movestogo && inc[si] == 0ms) {
            return time[si] / 25; // sudden death
        }

        auto moves = movestogo ? std::min(movestogo, 20) : 20;
        return inc[si] + (time[si]-inc[si])/moves;
    }

    void setSearchDeadline() {
        if (infinite.load(std::memory_order_relaxed)) {
            setNoDeadline();
            return;
        }
        if (movetime > 0ms) {
            setNoDeadline();
            deadline[HardDeadline] = movetime;
            return;
        }
        if (time[Side{My}] == 0ms) {
            setNoDeadline();
            return;
        }

        // average remaining time per move
        auto myAverage = average(Side{My}) + (canPonder ? average(Side{Op}) / 2 : 0ms);
        auto hardInterval = std::min(time[Side{My}], myAverage * 2) - moveOverhead;
        hardInterval = std::max(TimeInterval{0}, hardInterval);

        deadline[HardDeadline]      = hardInterval;   // 200% average time
        deadline[RootMoveDeadline]  = hardInterval/2; // 100% average time
        deadline[IterationDeadline] = hardInterval/4; // 50% average time
    }

    ReturnStatus refreshQuota() const {
        assertNodesOk();
        nodes -= nodesQuota;

        auto nodesRemaining = nodesLimit - nodes;
        if (nodesRemaining >= QuotaLimit) {
            nodesQuota = QuotaLimit;
        }
        else {
            nodesQuota = static_cast<decltype(nodesQuota)>(nodesRemaining);
            if (nodesQuota == 0) {
                assertNodesOk();
                return ReturnStatus::Stop;
            }
        }

        if (reached<HardDeadline>()) {
            nodesLimit = nodes;
            nodesQuota = 0;

            assertNodesOk();
            return ReturnStatus::Stop;
        }

        assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
        nodes += nodesQuota;
        --nodesQuota; //count current node

        assertNodesOk();
        return ReturnStatus::Continue;
    }

public:
    Ply depth{MaxPly};

    bool canPonder{false};

    TimeInterval moveOverhead = 0us;

    // clear all limits except canPonder and moveOverhead
    void clear() {
        nodes = 0;
        nodesLimit = NodeCountMax;
        nodesQuota = 0;
        searchStartTime = timeNow();
        setNoDeadline();
        movetime = 0ms;
        time = {{ 0ms, 0ms }};
        inc = {{ 0ms, 0ms }};
        depth = MaxPly;
        movestogo = 0;
        mate = 0;
        ponder.store(false, std::memory_order_relaxed);
        infinite.store(false, std::memory_order_relaxed);
        stop_.store(false, std::memory_order_relaxed);
    }

    istream& go(istream&, Side);

    void ponderhit() {
        ponder.store(false, std::memory_order_relaxed);
        reached<HardDeadline>();
    }

    void stop() const {
        infinite.store(false, std::memory_order_relaxed);
        ponder.store(false, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_release);
    }

    bool isStopped() const {
        return stop_.load(std::memory_order_acquire);
    }

    // ponder || infinite
    bool shouldDelayBestmove() {
        return ponder.load(std::memory_order_relaxed) || infinite.load(std::memory_order_relaxed);
    }

    template <deadline_t DeadlineKind>
    bool reached() const {
        if (isStopped()) { return true; }

        bool isDeadlineReached =
            !ponder.load(std::memory_order_relaxed)
            && deadline[DeadlineKind] != TimeInterval::max()
            && deadline[DeadlineKind] < elapsedSinceStart()
        ;

        if (isDeadlineReached) { stop(); }
        return isDeadlineReached;
    }

    TimeInterval elapsedSinceStart() const {
        return ::elapsedSince(searchStartTime);
    }

    /// exact number of visited nodes
    constexpr node_count_t getNodes() const {
        assertNodesOk();
        return nodes - nodesQuota;
    }

    ReturnStatus countNode() const {
        assertNodesOk();

        if (nodesQuota == 0 || isStopped()) {
            return refreshQuota();
        }

        assert (nodesQuota > 0);
        --nodesQuota;

        assertNodesOk();
        return ReturnStatus::Continue;
    }
};

class HistoryMoves {
    using Index = ::Index<2>;
    using _t = Side::arrayOf< PieceType::arrayOf<Square::arrayOf< Index::arrayOf<Move> >> >;
    _t v;
public:
    void clear() { std::memset(&v, 0, sizeof(v)); }
    const Move& get1 (Color c, PieceType ty, Square sq) const { return v[c][ty][sq][0]; }
    const Move& get2 (Color c, PieceType ty, Square sq) const { return v[c][ty][sq][1]; }
    void set(Color c, PieceType ty, Square sq, Move move) {
        if (v[c][ty][sq][0] != move) {
            v[c][ty][sq][1] = v[c][ty][sq][0];
            v[c][ty][sq][0] = move;
        }
    }
};

// triangular array
class CACHE_ALIGN PvMoves {
    static constexpr auto triangularArraySize = (Ply::Last+1) * (Ply::Last+2) / 2;
public:
    using Index = ::Index<triangularArraySize>;
    Index::arrayOf<UciMove> pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void clearPly(Index i) { pv[i] = UciMove{}; }

    Index set(Index parent, UciMove move, Index child) {
        pv[parent++] = move;
        assert (parent <= child);
        while ((pv[parent++] = pv[child++]));
        pv[parent] = UciMove{};
        return parent; // new child index
    }

    operator const UciMove* () const { return &pv[0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << static_cast<const UciMove*>(pvMoves);
    }
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
    UciPosition position_; // result of parsing 'position' command
    UciSearchLimits limits; // result of parsing 'go' command

    mutable Tt tt;
    Repetitions repetitions;
    mutable PvMoves pvMoves;
    mutable Score pvScore{NoScore};
    mutable HistoryMoves counterMove;
    mutable HistoryMoves followMove;

private:
    Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

    // output stream
    ostream& out;
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    std::string bestmove_;
    std::mutex bestmoveMutex;

    // avoid race conditions betweeen Uci output and main search thread output

    // avoid printing identical 'info nps' lines in a row
    mutable node_count_t lastInfoNodes = 0;

    std::string logFileName; // no log by default
    mutable std::ofstream logFile;
    mutable std::mutex logMutex;
    const int pid; // system process id (for debug output)

#ifdef NDEBUG
    bool isDebugOn = false;
#else
    bool isDebugOn = true;
#endif

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

    //UCI command handlers

    void uciok() const;
    void setoption();
    void setHash();
    void ucinewgame();
    void position();

    void readyok() const;
    void go();
    void stop();
    void ponderhit();
    void bestmove();

    void debug();
    void goPerft();
    void info_perft_bestmove();

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(const std::string&) const;

    // log messages to the logFile named by logFileName
    void log(const std::string&) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = Ply{0}) const { return position_.colorToMove(ply); }

    void newGame() {
        tt.newGame();
        counterMove.clear();
        followMove.clear();
    }

    void newSearch() {
        limits.clear();
        tt.newSearch();
        pvMoves.clear();
        pvScore = Score{NoScore};
    }

    void newIteration() const {
        tt.newIteration();
    }

    void refreshTtPv(Ply depth) const;

    void info_pv(Ply) const;
    void info_iteration(Ply) const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t) const;
};

#endif
