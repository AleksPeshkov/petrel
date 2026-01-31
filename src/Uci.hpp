#ifndef UCI_HPP
#define UCI_HPP

#include <atomic>
#include <fstream>
#include <mutex>
#include "chrono.hpp"
#include "history.hpp"
#include "io.hpp"
#include "Index.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciPosition.hpp"

// EasyMove = 2/3, NormalMove = 1, HardMove = 3/2
enum move_complexity_t { MoveTime = 0, EasyMove = 4, NormalMove = 6, HardMove = 9 };

// IterationDeadline = 1/2, HardDeadline = 3
enum deadline_t { IterationDeadline = 1, AverageScale = 2, HardDeadline = 6 };

class UciSearchLimits {
    constexpr static TimeInterval NoDeadline = TimeInterval::max();
    constexpr static node_count_t NodeCountMax = std::numeric_limits<node_count_t>::max();
    constexpr static int QuotaLimit = 1000;

    mutable node_count_t nodes_{0}; // (0 <= nodes_ && nodes_ <= nodesLimit_)
    mutable node_count_t nodesLimit_{NodeCountMax}; // search limit

    //number of remaining nodes before slow checking for search stop
    mutable int nodesQuota_{0}; // (0 <= nodesQuota_ && nodesQuota_ <= QuotaLimit)

    mutable std::atomic_bool stop_{false};
    mutable std::atomic_bool infinite_{false};
    mutable std::atomic_bool ponder_{false};

    TimePoint searchStartTime_;

    TimeInterval deadline_{NoDeadline};

    Side::arrayOf<TimeInterval> time_{ 0ms, 0ms };
    Side::arrayOf<TimeInterval> inc_{ 0ms, 0ms };
    TimeInterval movetime_{0ms};

    int movestogo_{0};

    void clearDeadline() {
        deadline_ = NoDeadline;
        moveComplexity = MoveTime;
    }

    constexpr bool isNoDeadline() const { return deadline_ == NoDeadline; }

public:
    Ply depth{MaxPly};

    mutable move_complexity_t moveComplexity{MoveTime};
    mutable UciMove easyMove{};

    TimeInterval moveOverhead{0us};
    bool canPonder{false};

    // clear all limits except canPonder and moveOverhead
    void clear();

    bool isStopped() const { return stop_.load(std::memory_order_acquire); }

    // ponder || infinite
    bool shouldDelayBestmove() const {
        return ponder_.load(std::memory_order_relaxed) || infinite_.load(std::memory_order_relaxed);
    }

    TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime_); }

    template <deadline_t Deadline>
    bool reached() const {
        if (isStopped()) { return true; }
        if (nodes_ == 0 || isNoDeadline() || ponder_.load(std::memory_order_relaxed)) { return false; }
        if (moveComplexity == MoveTime && Deadline != HardDeadline) { return false; }

        TimeInterval current = deadline_;
        assert (moveComplexity != MoveTime);
        current *= static_cast<int>(moveComplexity) * Deadline;
        current /= static_cast<int>(NormalMove) * HardDeadline;

        bool isDeadlineReached = current < elapsedSinceStart();
        if (isDeadlineReached) { stop(); }
        return isDeadlineReached;
    }

    void ponderhit() {
        ponder_.store(false, std::memory_order_relaxed);
        reached<HardDeadline>();
    }

    void stop() const {
        stop_.store(true, std::memory_order_release);
        infinite_.store(false, std::memory_order_relaxed);
        ponder_.store(false, std::memory_order_relaxed);
    }

    constexpr TimeInterval average(Side si) const;
    void setSearchDeadline();
    istream& go(istream&, Side);

    constexpr void assertNodesOk() const {
        assert (0 <= nodesQuota_);
        assert (nodesQuota_ < QuotaLimit);
        //assert (0 <= nodes);
        assert (nodes_ <= nodesLimit_);
        assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
    }

    // exact number of visited nodes
    constexpr node_count_t getNodes() const {
        assertNodesOk();
        return nodes_ - nodesQuota_;
    }

// defined and used in search.cpp:

    ReturnStatus countNode() const;
    ReturnStatus refreshQuota() const;
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
    UciPosition position_; // result of parsing 'position' command
    UciSearchLimits limits; // result of parsing 'go' command
    Repetitions repetitions;

    mutable HistoryMoves<4> counterMove;
    mutable HistoryMoves<4> followMove;

    mutable PvMoves pvMoves;
    mutable Score pvScore{NoScore};

    mutable Tt tt; // main transposition table

    // the last move is the most recent one
    mutable std::vector<UciMove> rootBestMoves;

private:
    Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

#ifdef ENABLE_ASSERT_LOGGING
    friend void assert_fail(const char*, const char*, unsigned, const char*);
    std::string debugPosition;
    std::string debugGo;
#endif

    // output stream
    ostream& out;
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    mutable std::string bestmove_;
    mutable std::mutex bestmoveMutex;

    // avoid race conditions betweeen Uci output and main search thread output

    // avoid printing identical 'info nps' lines in a row
    mutable node_count_t lastInfoNodes = 0;

    // the first move after ucinewgame will spend more thinking time
    bool isNewGame = true;

    std::string logFileName; // no log by default
    mutable std::ofstream logFile;
    mutable std::mutex logMutex;
    bool isDebugOn = false;

    std::string evalFileName; // use embedded by default

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

    void go();
    void goPerft();
    void stop();
    void ponderhit();

    void bench();
    void debug();

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

    void loadEvalFile(const std::string&);
    void setEmbeddedEval();

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(std::string_view) const;

    // log messages to the logFile named by logFileName
    void log(std::string_view) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = Ply{0}) const { return position_.colorToMove(ply); }

    void bench(std::string& goLimits);

    void newGame() {
        tt.newGame();
        counterMove.clear();
        followMove.clear();
        isNewGame = true;
    }

    void newSearch() {
        limits.clear();
        tt.newSearch();
        pvMoves.clear();
        pvScore = Score{NoScore};
        rootBestMoves.clear();
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
