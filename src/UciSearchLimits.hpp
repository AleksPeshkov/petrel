#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include <atomic>
#include <chrono>
#include "io.hpp"
#include "Index.hpp"

using namespace std::chrono_literals;

using clock_type = std::chrono::steady_clock;
using TimePoint = clock_type::time_point;
using TimeInterval = clock_type::duration;

inline TimePoint timeNow() { return clock_type::now(); }

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
}

// ::timeNow() - start
inline TimeInterval elapsedSince(TimePoint start) { return ::timeNow() - start; }

class Position;

class UciSearchLimits {
    // IterationDeadline = ~1/2, HardDeadline = ~3x of averageMoveTime
    enum deadline_t { IterationDeadline = 11, AverageTimeScale = 20, HardDeadline = 64 };

    // EasyMove = 3/5, HardMove = 8/5 (Fibonacci numbers)
    enum move_control_t { ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8 };

    constexpr static TimeInterval UnlimitedTime{TimeInterval::max()};
    constexpr static node_count_t NodeCountMax{std::numeric_limits<node_count_t>::max()};
    constexpr static int QuotaLimit{1000};

    mutable node_count_t nodes_{0}; // (0 <= nodes_ && nodes_ <= nodesLimit_)
    mutable node_count_t nodesLimit_{NodeCountMax}; // search limit

    // avoid output duplicate 'info nps'
    mutable node_count_t lastInfoNodes_ = 0;

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

    // iteration time low material bonus -10% | 0 | +10% | 20% | 30%
    // less pieces remain: the better BF, the less time to finish iteration needed
    int iterLowMaterialBonus_{0};

    Side::arrayOf<TimeInterval> time_{ 0ms, 0ms };
    Side::arrayOf<TimeInterval> inc_{ 0ms, 0ms };
    TimeInterval movetime_{0ms};
    int movestogo_{0};

    Ply maxDepth_{MaxPly};

    mutable move_control_t timeControl_{ExactTime}; // ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8
    mutable UciMove easyMove_{}; // previous root best move (for EasyMove / NormalMove / HardMove strategy)
    bool isNewGame_{true}; // the first move after ucinewgame will spend more thinking time

// UCI configurable options
    constexpr static TimeInterval MoveOverheadDefault{200us};
    TimeInterval moveOverhead_{MoveOverheadDefault};
    bool canPonder_{false};

    void assertNodesOk() const;

// unify sudden death, increment and cyclic time controls
    int lookAheadMoves() const;
    TimeInterval lookAheadTime(Side) const;
    TimeInterval averageMoveTime(Side) const;
    void setSearchDeadlines(const Position* = nullptr);

    template <deadline_t Deadline> bool reachedTime() const;
    TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime_); }

    ReturnStatus refreshQuota() const;
    bool isStopped() const { return timeout_.load(std::memory_order_seq_cst); }

public:
// called from the Uci input handling thread:
    void newGame() { isNewGame_ = true; newSearch(); }
    void newSearch(); // start search timer, clear all limits except canPonder_, moveOverhead_ and isNewGame_
    void setoption(istream&);
    void go(istream&, Side, const Position* = nullptr);
    void stop();
    void ponderhit();
    ostream& uciok(ostream&) const;

    // ponder || infinite
    bool shouldDelayBestmove() const { return pondering_.load(std::memory_order_release) || infinite_; }

    bool canPonder() const { return canPonder_; }

    constexpr node_count_t getNodes() const { return nodes_ - nodesQuota_; } // exact number of searched nodes
    bool hasNewNodes() const { return lastInfoNodes_ != getNodes(); }
    ostream& info_nps(ostream&) const;

// used in search.cpp:
    ReturnStatus countNode() const; // checks for search stop reasons
    constexpr Ply maxDepth() const { return maxDepth_; }
    bool hardDeadlineReached() const;
    bool iterationDeadlineReached() const;
    void updateMoveComplexity(UciMove bestMove) const;
};

#endif
