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

class UciSearchLimits {
    constexpr static TimeInterval UnlimitedTime{TimeInterval::max()};
    constexpr static node_count_t NodeCountMax{std::numeric_limits<node_count_t>::max()};
    constexpr static int QuotaLimit{1000};

    mutable node_count_t nodes_{0}; // (0 <= nodes_ && nodes_ <= nodesLimit_)
    mutable node_count_t nodesLimit_{NodeCountMax}; // search limit

    // number of remaining nodes before slow checking for search stop
    // (0 <= nodesQuota_ && nodesQuota_ <= QuotaLimit)
    mutable int nodesQuota_{0};

    // set either by UCI 'stop' command or by the search thread internal check of either search limit reached
    mutable std::atomic_bool timeout_{false};

    // 'go ponder' mode, changed by the input thread, read by both search and input
    mutable std::atomic_bool pondering_{false};

    // 'go infinite' mode, non atomic, used only by the input thread itself
    mutable bool infinite_{false};

    TimePoint searchStartTime_{};

    // maximum move thinking time
    TimeInterval timePool_{UnlimitedTime};

    Side::arrayOf<TimeInterval> time_{ 0ms, 0ms };
    Side::arrayOf<TimeInterval> inc_{ 0ms, 0ms };
    TimeInterval movetime_{0ms};
    int movestogo_{0};

    Ply maxDepth_{MaxPly};

    // the first move after ucinewgame will spend more thinking time
    bool isNewGame_{true};

    constexpr void assertNodesOk() const;

    // time allocation strategy
    static constexpr int LookAheadMoves = 16;

    constexpr int lookAheadMoves() const {
        return movestogo_ > 0 ? std::min(movestogo_, LookAheadMoves) : LookAheadMoves;
    }

    // unify sudden death, increment and cyclic time controls
    constexpr TimeInterval lookAheadTime(Side si) const {
        return time_[si] + inc_[si] * (lookAheadMoves() - 1);
    }

    constexpr TimeInterval averageMoveTime(Side si) const {
        return lookAheadTime(si) / lookAheadMoves();
    }

    void setSearchDeadlines();

    ReturnStatus refreshQuota() const;
    bool isStopped() const { return timeout_.load(std::memory_order_seq_cst); }

    // IterationDeadline = ~1/2, HardDeadline = ~3x of averageMoveTime
    enum deadline_t { IterationDeadline = 11, AverageTimeScale = 20, HardDeadline = 64 };
    template <deadline_t Deadline> bool reached() const;

    // EasyMove = 3/5, HardMove = 8/5 (Fibonacci numbers)
    enum move_control_t { ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8 };

    // ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8
    mutable move_control_t timeControl_{ExactTime};

    // determine EasyMove / NormalMove / HardMove
    mutable UciMove easyMove_{}; // prvious root best move

public:
// UCI configurable options

    constexpr static TimeInterval MoveOverheadDefault{200us};
    TimeInterval moveOverhead_{MoveOverheadDefault};
    bool canPonder_{false};

    void newGame() { isNewGame_ = true; newSearch(); }

    // start search timer, clear all limits except canPonder_, moveOverhead_ and isNewGame_
    void newSearch();

    TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime_); }

    // ponder || infinite
    bool shouldDelayBestmove() const {
        return pondering_.load(std::memory_order_release) || infinite_;
    }

    // exact number of visited nodes
    constexpr node_count_t getNodes() const { return nodes_ - nodesQuota_; }

    constexpr Ply maxDepth() const { return maxDepth_; }

// called from the Uci input handling thread:

    istream& go(istream&, Side);
    void stop();
    void ponderhit();

// defined and used in search.cpp:

    bool hardDeadlineReached() const { return reached<HardDeadline>(); }
    bool iterationDeadlineReached() const { return reached<IterationDeadline>(); }

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

    void sendDelayedBestMove() const;
    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and if isDebugOn to log file
    void output(std::string_view) const;

    // output to cerr, uci info string, log file
    void error(std::string_view) const;

    // output to uci info string, log file
    void info(std::string_view) const;

    // log messages to the logFile named by logFileName
    void log(std::string_view) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return position_.colorToMove(ply); }

    void bench(std::string& goLimits);

    void newGame() {
        limits.newGame();
        tt.newGame();
        counterMove.clear();
        followMove.clear();
    }

    void newSearch() {
        {
            std::lock_guard<std::mutex> lock{bestmoveMutex};
            if (!bestmove_.empty()) {
                log("#New search started, but bestmove is not empty: " + bestmove_);
                bestmove_.clear();
            }
        }
        limits.newSearch();
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
