#ifndef SEARCH_LIMITS_HPP
#define SEARCH_LIMITS_HPP

#include <atomic>
#include <chrono>
#include "Score.hpp"

using namespace std::chrono_literals;

using clock_type = std::chrono::steady_clock;
using TimePoint = clock_type::time_point;
using TimeInterval = clock_type::duration;

inline TimePoint timeNow() { return clock_type::now(); }

// ::timeNow() - start
inline TimeInterval elapsedSince(TimePoint start) { return ::timeNow() - start; }

class PrincipalVariation;
struct UciLimits;
class UciPosition;

class SearchLimits {
    // thinking time pool scaled to OptimumTimeQuota = 100% of averageMoveTime()
    enum time_quota_t { IterationQuota = 12, OptimumTimeQuota = 20, MaxQuota = 64 };

    // NormalMove time = 100% of averageMoveTime(), EasyMove = 3/5, HardMove = 8/5 (Fibonacci numbers)
    enum time_strategy_t { ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8 };

    static constexpr TimeInterval UnlimitedTime{TimeInterval::max()};
    static constexpr node_count_t NodeCountMax{std::numeric_limits<node_count_t>::max()};
    static constexpr int LookAheadMoves{16}; // number of moves to allocate time for
    static constexpr int QuotaLimit{1000};

    node_count_t nodes_{0}; // (0 <= nodes_ && nodes_ <= nodesLimit_)
    node_count_t nodesLimit_{NodeCountMax}; // search limit

    // number of remaining nodes before (slow) checking for time deadline and UCI stop
    // (0 <= nodesQuota_ && nodesQuota_ <= QuotaLimit)
    int nodesQuota_{0};

    Ply maxDepth_{MaxPly}; // go depth

    // set by 'stop' UCI command, read by search
    std::atomic_bool stop_{false};

    // 'go ponder' mode, reset by 'stop' or 'ponderhit' UCI command, read by both search and input
    std::atomic_bool pondering_{false};

    TimePoint searchStartTime_{}; // reset in newSearch()
    TimeInterval timePool_{UnlimitedTime}; // maximum move thinking time

// dynamic time management:

    // root position low material iteration time bonus -10% | 0 | +10% | 20% | 30%
    // less pieces remain, the better BF, the less time to finish iteration needed in average
    int lowMaterialQuotaBonus_{0};

    time_strategy_t timeStrategy_{ExactTime}; // ExactTime = 0, EasyMove = 3, NormalMove = 5, HardMove = 8
    Move  lastMove_{}; // last best root move (for updating timeStrategy_)
    Score lastScore_{NoScore}; // last best root move score (for updating timeStrategy_)
    Ply hardMoveDepth_{0}; // iteration when HardMove triggered

private:
    template <time_quota_t TimeQuota>
    [[nodiscard]] ReturnStatus reachedTime() const;

    void assertNodesOk() const;
    ReturnStatus refreshQuota();

public:
    constexpr Ply maxDepth() const { return maxDepth_; }
    constexpr node_count_t getNodes() const { return nodes_ - nodesQuota_; } // exact number of searched nodes

// called from the Uci input handling thread:
    TimePoint newSearch(); // clear search state
    void setLimits(const UciLimits&, const UciPosition&);
    void stop();
    void ponderhit();

    bool pondering() const { return pondering_.load(std::memory_order_relaxed); }
    auto searchStartTime() const { return searchStartTime_; }

// used in search.cpp:
    // checks for search stop reasons
    [[nodiscard]] ReturnStatus countNode() {
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
    [[nodiscard]] ReturnStatus updateTimeStrategy(const PrincipalVariation&);
};

#endif
