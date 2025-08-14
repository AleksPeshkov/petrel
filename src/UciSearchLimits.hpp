#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "Thread.hpp"

class UciSearchLimits {
public:
    TimePoint searchStartTime;

    TimeInterval deadline = TimeInterval::max();
    TimeInterval movetime = 0ms;

    Color::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Color::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};

    node_count_t nodes = NodeCountMax;
    Ply depth = MaxPly;

    index_t movestogo = 0;
    index_t mate = 0;

    bool ponder = false;
    bool infinite = false;

    bool canPonder = false;
    TimeInterval moveOverhead = 0ms;

    // clear all limits except canPonder and moveOverhead
    void clear() {
        searchStartTime = timeNow();
        deadline = TimeInterval::max();
        movetime = 0ms;
        time = {{ 0ms, 0ms }};
        inc = {{ 0ms, 0ms }};
        nodes = NodeCountMax;
        depth = MaxPly;
        movestogo = 0;
        mate = 0;
        ponder = false;
        infinite = false;
    }

    // no time limits
    constexpr TimeInterval setNoDeadline() { return deadline = TimeInterval::max(); }
    constexpr bool hasDeadline() const { return deadline != TimeInterval::max(); }

    constexpr TimeInterval average(Side my) const {
        auto moves = movestogo ? movestogo : 30;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

    constexpr TimeInterval calculateDeadline() {
        if (infinite) { return setNoDeadline(); }
        if (ponder) { return setNoDeadline(); }
        if (movetime > 0ms) { return deadline = movetime - moveOverhead; }

        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);
        if (myAverage == 0ms) { return setNoDeadline(); } // go without any limits

        return deadline = std::min(time[My], myAverage) - moveOverhead;
    }

    void ponderhit(ThreadWithDeadline& searchThread) {
        ponder = false;
        setDeadline(searchThread);
    }

    void setDeadline(ThreadWithDeadline& searchThread) {
        calculateDeadline();
        if (!hasDeadline()) { return; }

        if (deadline < 500us) {
            // we have got no time to think
            // set search limit to depth == 1 to return at least a valid best move
            depth = 1;
            return;
        }
        searchThread.setDeadline(searchStartTime + deadline - 500us);
    }

    constexpr bool softDeadlineReached() const {
        if (!hasDeadline()) { return false; }
        auto softDeadline = deadline * 3 / 4;
        return ::elapsedSince(searchStartTime) > softDeadline;
    }
};

#endif
