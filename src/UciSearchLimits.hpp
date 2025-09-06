#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "Thread.hpp"

class UciSearchLimits {
public:
    TimePoint searchStartTime;
    TimePoint hardDeadline = TimePoint::max(); // tested every 100 nodes
    TimePoint iterationDeadline = TimePoint::max(); // tested when iteration ends
    TimePoint updatePvDeadline = TimePoint::max(); // tested when pv updates at root

    TimeInterval movetime = 0ms;

    Side::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Side::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};

    node_count_t nodes = NodeCountMax;
    Ply depth = {MaxPly};

    int movestogo = 0;
    int mate = 0;

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
        depth = {MaxPly};
        movestogo = 0;
        mate = 0;
        ponder = false;
        infinite = false;
    }

    constexpr void setNoDeadline() {
        hardDeadline = TimePoint::max();
        iterationDeadline = TimePoint::max();
        updatePvDeadline = TimePoint::max();
    }

    constexpr TimeInterval average(Side my) const {
        auto moves = (movestogo > 0) ? movestogo : 30;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

    constexpr void setSearchDeadline(TimeInterval ponderElapsed = 0ms) {
        if (infinite || ponder) {
            setNoDeadline();
            return;
        }

        if (movetime > 0ms) {
            setNoDeadline();
            hardDeadline = searchStartTime + movetime - moveOverhead;
            return;
        }

        if (time[My] == 0ms) {
            setNoDeadline();
            return;
        }

        // average remaining time per move
        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);

        auto hardInterval = std::max<TimeInterval>(0ms, std::min(time[My], myAverage * 6 / 5) - moveOverhead); // 120% average time

        if (hardInterval > 100us || ponderElapsed > 100us) {
            // normal time management
            hardDeadline = searchStartTime + hardInterval;
            iterationDeadline = searchStartTime + (hardInterval / 2); // 60% average time
            updatePvDeadline = searchStartTime + (hardInterval * 3 / 4); // 90% average time
            return;
        }

        io::log("#hardInterval too small");

        // almost no time left
        // return hash move or finish iteration 1 to get a reasonable best move
        setNoDeadline();
        iterationDeadline = searchStartTime;
    }

    void ponderhit() {
        if (!ponder) { return; } // ignore ponderhit if not pondering

        ponder = false;
        auto elapsed = ::elapsedSince(searchStartTime);
        time[Op] -= std::min(elapsed, time[Op]);
        setSearchDeadline(elapsed);
    }

    constexpr bool hardDeadlineReached() const {
        return hardDeadline != TimePoint::max() && hardDeadline < ::timeNow();
    }

    constexpr bool iterationDeadlineReached() const {
        return iterationDeadline != TimePoint::max() && iterationDeadline < ::timeNow();
    }

    constexpr bool updatePvDeadlineReached() const {
        return updatePvDeadline != TimePoint::max() && updatePvDeadline < ::timeNow();
    }
};

#endif
