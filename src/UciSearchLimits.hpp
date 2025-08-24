#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "Thread.hpp"

class UciSearchLimits {
public:
    TimePoint searchStartTime;
    TimePoint hardDeadline = TimePoint::max();
    TimePoint softDeadline = TimePoint::max();

    TimeInterval movetime = 0ms;

    Side::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Side::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};

    node_count_t nodes = NodeCountMax;
    Ply depth = MaxPly;

    index_t movestogo = 0;
    index_t mate = 0;

    bool ponder = false;
    bool infinite = false;

    bool canPonder = false;
    TimeInterval moveOverhead = 100us;

    // clear all limits except canPonder and moveOverhead
    void clear() {
        searchStartTime = timeNow();
        setNoDeadline();
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

    constexpr void setNoDeadline() {
        hardDeadline = TimePoint::max();
        softDeadline = TimePoint::max();
    }

    constexpr TimeInterval average(Side my) const {
        auto moves = movestogo ? movestogo : 30;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

    constexpr void calculateDeadline() {
        if (infinite || ponder) {
            setNoDeadline();
            return;
        }

        if (movetime > 0ms) {
            hardDeadline = searchStartTime + movetime - moveOverhead;
            softDeadline = TimePoint::max();
            return;
        }

        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);
        if (myAverage == 0ms) {
            // 'go' command without any time limits
            setNoDeadline();
            return;
        }

        auto timeInterval = std::min(time[My], myAverage) - moveOverhead;
        hardDeadline = searchStartTime + timeInterval;
        softDeadline = searchStartTime + (timeInterval * 3 / 4);
    }

    void ponderhit() {
        ponder = false;
        time[Op] -= std::min(::elapsedSince(searchStartTime), time[Op]);
        setSearchDeadline();
    }

    void setSearchDeadline() {
        calculateDeadline();
        if (hardDeadline == TimePoint::max()) { return; }

        auto now = ::timeNow();
        if (hardDeadline < now + 100us) {
            // have got almost 0 time to think, search at least 1 root move to get minimal legal PV
            hardDeadline = TimePoint::max();
            softDeadline = now;
            return;
        }
    }

    constexpr bool hardDeadlineReached() const {
        return hardDeadline != TimePoint::max() && hardDeadline < ::timeNow();
    }

    constexpr bool softDeadlineReached() const {
        return softDeadline != TimePoint::max() && softDeadline < ::timeNow();
    }
};

#endif
