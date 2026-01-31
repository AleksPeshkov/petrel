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

// EasyMove = 5/8, HardMove = 13/8 (Fibonacci numbers)
enum move_complexity_t { MoveTime = 0, EasyMove = 5, NormalMove = 8, HardMove = 13 };

// IterationDeadline = 1/2, HardDeadline = 3
enum deadline_t { IterationDeadline = 1, AverageScale = 2, HardDeadline = 6 };

class UciSearchLimits {
    constexpr static TimeInterval NoDeadline{TimeInterval::max()};
    constexpr static node_count_t NodeCountMax{std::numeric_limits<node_count_t>::max()};
    constexpr static int QuotaLimit{1000};

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

    Ply maxDepth_{MaxPly};

    void clearDeadline() {
        deadline_ = NoDeadline;
        moveComplexity = MoveTime;
    }

    constexpr void assertNodesOk() const;

    void setSearchDeadline(bool extraTime);
    constexpr TimeInterval average(Side si) const;

    ReturnStatus refreshQuota() const;

    mutable move_complexity_t moveComplexity{MoveTime};
    mutable UciMove easyMove{}; // prvious root best move

public:
// UCI configurable options

    constexpr static TimeInterval MoveOverheadDefault{200us};
    TimeInterval moveOverhead{MoveOverheadDefault};
    bool canPonder{false};

    // clear all limits except canPonder and moveOverhead
    void clear();

    bool isStopped() const { return stop_.load(std::memory_order_acquire); }
    TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime_); }

    // ponder || infinite
    bool shouldDelayBestmove() const {
        return ponder_.load(std::memory_order_relaxed) || infinite_.load(std::memory_order_relaxed);
    }

    // exact number of visited nodes
    constexpr node_count_t getNodes() const { return nodes_ - nodesQuota_; }

    constexpr Ply maxDepth() const { return maxDepth_; }

// called from the Uci input handling thread:

    istream& go(istream&, Side, bool extraTime = false);
    void stop();
    void ponderhit();

// defined and used in search.cpp:

    template <deadline_t Deadline> bool reached() const;
    ReturnStatus countNode() const;
    void updateMoveComplexity(UciMove bestMove) const;
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
    void ucinewgame();
    void position();
    void go();
    void stop();
    void ponderhit();
    void setoption();

    void setHash();
    void setdebug();

    void goPerft();
    void bench();

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(std::string_view) const;

    // log messages to the logFile named by logFileName
    void log(std::string_view) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return position_.colorToMove(ply); }

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
    }

    void newIteration() const {
        tt.newIteration();
    }

    void refreshTtPv(Ply depth) const;

    void info_pv(Ply) const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, UciMove currentMove, node_count_t) const;
};

#endif
