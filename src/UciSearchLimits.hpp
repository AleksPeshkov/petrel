#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "PiBb.hpp"

class UciSearchLimits {
public:
    // thinkingTime == 0 means no time limit
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

    void calculateThinkingTime() {
        if (movetime != TimeInterval::zero()) {
            thinkingTime = movetime;
            return;
        }

        auto moves = movestogo ? movestogo : 40;
        auto averageTime = (time[My] + (moves-1)*inc[My]) / moves;
        thinkingTime = std::min(time[My], averageTime);
    }
};

#endif
