#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "PiBb.hpp"

class UciSearchLimits {
public:
    // thinkingTime == 0 means infinite search
    TimeInterval thinkingTime = 0ms;

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
        auto moves = movestogo ? movestogo : 40;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

    TimeInterval calculateThinkingTime(bool canPonder) {
        if (infinite) { return thinkingTime = 0ms; }
        if (ponder) { assert(canPonder); return thinkingTime = 0ms; }

        if (movetime > 0ms) { return thinkingTime = movetime; }

        auto averageTime = average(My);
        if (canPonder) { averageTime += average(Op) / 2; }
        thinkingTime = std::min(time[My], averageTime);

        if (thinkingTime == 0ms) { infinite = true; }
        return thinkingTime;
    }
};

#endif
