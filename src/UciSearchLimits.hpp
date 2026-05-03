#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include <atomic>
#include <chrono>
#include "io.hpp"
#include "Score.hpp"

#define RETURN_IF_STOP(visitor) { if (visitor == ReturnStatus::Stop) { return ReturnStatus::Stop; } } ((void)0)

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
class UciPosition;
class PrincipalVariation;

class UciSearchLimits {
    // thinking time pool scaled to OptimumTimeQuota = 100% of averageMoveTime()
    enum time_quota_t { IterationQuota = 12, OptimumTimeQuota = 20, MaxQuota = 64 };

    // NormalMove time = 100% of averageMoveTime(), EasyMove = 3/5, HardMove = 8/5 (Fibonacci numbers)
    enum time_strategy_t { ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8 };

    static constexpr TimeInterval UnlimitedTime{TimeInterval::max()};
    static constexpr node_count_t NodeCountMax{std::numeric_limits<node_count_t>::max()};
    static constexpr int QuotaLimit{1000};
    static constexpr TimeInterval MoveOverheadDefault{1ms};

    mutable node_count_t nodes_{0}; // (0 <= nodes_ && nodes_ <= nodesLimit_)
    mutable node_count_t nodesLimit_{NodeCountMax}; // search limit

    mutable node_count_t lastInfoNodes_{0}; // avoid output duplicate 'info nps'
    mutable TimePoint lastInfoTime_{}; // for instantaneous nps

    // number of remaining nodes before (slow) checking for time deadline and UCI stop
    // (0 <= nodesQuota_ && nodesQuota_ <= QuotaLimit)
    mutable int nodesQuota_{0};

// UCI configurable options and go limits
    TimeInterval moveOverhead_{MoveOverheadDefault};
    Ply maxDepth_{MaxPly}; // go depth
    bool isNewGame_{true}; // the first search after ucinewgame
    bool canPonder_{false}; // option canPonder
    bool infinite_{false}; // 'go infinite' mode, non atomic, used only by the input thread itself

    // set by 'stop' UCI command, read by search
    std::atomic_bool stop_{false};

    // 'go ponder' mode, reset by 'stop' or 'ponderhit' UCI command, read by both search and input
    std::atomic_bool pondering_{false};

    array<TimeInterval, Side> time_{ 0ms, 0ms }; // go wtime, go btime
    array<TimeInterval, Side> inc_{ 0ms, 0ms }; // go winc, go binc
    TimeInterval movetime_{0ms}; // go movetime
    int movestogo_{0}; // go movestogo

    TimePoint searchStartTime_{}; // reset in newSearch()
    TimeInterval timePool_{UnlimitedTime}; // maximum move thinking time

// dynamic time management:

    // root position low material iteration time bonus -10% | 0 | +10% | 20% | 30%
    // less pieces remain, the better BF, the less time to finish iteration needed in average
    int lowMaterialQuotaBonus_{0};

    mutable time_strategy_t timeStrategy_{ExactTime}; // ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8
    mutable HistoryMove lastMove_{}; // last best root move (for updating timeStrategy_)
    mutable Score lastScore_{NoScore}; // last best root move score (for updating timeStrategy_)
    mutable Ply hardMoveDepth_{0}; // iteration when HardMove triggered

private:
    int lookAheadMoves() const; // unify sudden death, increment and cyclic time controls
    TimeInterval lookAheadTime(Side) const; // unify sudden death, increment and cyclic time controls
    TimeInterval averageMoveTime(Side) const; // unify sudden death, increment and cyclic time controls
    TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime_); }

    void setTimeDeadlines(const Position&);

    template <time_quota_t TimeQuota>
    [[nodiscard]] ReturnStatus reachedTime() const;

    void assertNodesOk() const;
    ReturnStatus refreshQuota() const;

public:
    constexpr bool canPonder() const { return canPonder_; }
    constexpr Ply maxDepth() const { return maxDepth_; }
    constexpr node_count_t getNodes() const { return nodes_ - nodesQuota_; } // exact number of searched nodes

// called from the Uci input handling thread:
    ostream& average_nps(ostream&) const;
    ostream& instant_nps(ostream&) const;
    ostream& uciok(ostream&) const;

    void setoption(istream&);
    void go(istream&, UciPosition&);
    void stop();
    void ponderhit();

    void newGame() { isNewGame_ = true; }
    void newSearch(); // start search timer, clear all except canPonder_, moveOverhead_ and isNewGame_

    // pondering || infinite
    bool shouldDelayBestmove() const { return pondering_.load(std::memory_order_relaxed) || infinite_; }
    constexpr bool hasNewNodes() const { return lastInfoNodes_ != getNodes(); }

// used in search.cpp:
    // checks for search stop reasons
    ReturnStatus countNode() const {
        assertNodesOk();

        if (nodesQuota_ <= 0) {
            RETURN_IF_STOP (refreshQuota());
        }

        assert (nodesQuota_ > 0);
        --nodesQuota_;

        assertNodesOk();
        return ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus lastDeadlineReached() const;
    [[nodiscard]] ReturnStatus iterationDeadlineReached() const;
    [[nodiscard]] ReturnStatus updateTimeStrategy(const PrincipalVariation&) const;
};

#endif
