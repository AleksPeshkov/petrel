#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "PiBb.hpp"

class UciSearchLimits {
public:
    TimePoint searchStartTime;

    // hardDeadline == 0 means no time limit
    TimeInterval hardDeadline = 0ms;

    // softDeadline == 0 means use all time
    TimeInterval softDeadline = 0ms;

    Color::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Color::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};

    TimeInterval movetime = 0ms;

    node_count_t nodes = NodeCountMax;
    Ply depth = MaxPly;

    index_t movestogo = 0;
    index_t mate = 0;

    bool ponder = false;
    bool infinite = false;

    constexpr TimeInterval average(Side my) const {
        auto moves = movestogo ? movestogo : 30;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

    TimeInterval calculateThinkingTime(bool canPonder) {
        if (infinite) { return hardDeadline = 0ms; }
        if (ponder) { return hardDeadline = 0ms; }
        if (movetime > 0ms) { return hardDeadline = movetime; }

        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);
        hardDeadline = std::min(time[My], myAverage);
        softDeadline = hardDeadline * 3 / 4;

        return hardDeadline;
    }

    TimeInterval ponderhit() {
        ponder = false;
        return calculateThinkingTime(true);
    }

    bool softDeadlineReached() const {
        if (softDeadline == 0ms) { return false; }
        return ::elapsedSince(searchStartTime) > softDeadline;
    }
};

#endif
