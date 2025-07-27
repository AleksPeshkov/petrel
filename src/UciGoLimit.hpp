#ifndef UCI_GO_LIMIT_HPP
#define UCI_GO_LIMIT_HPP

#include "typedefs.hpp"
#include "chrono.hpp"
#include "PositionFen.hpp"

class UciGoLimit {
public:
    PositionFen positionMoves;

    Color::arrayOf<TimeInterval> time = {{ TimeInterval::zero(), TimeInterval::zero() }};
    Color::arrayOf<TimeInterval> inc = {{ TimeInterval::zero(), TimeInterval::zero() }};

    TimeInterval movetime = TimeInterval::zero();

    node_count_t nodes = NodeCountMax;
    Ply depth = MaxPly;

    index_t movestogo = 0;
    index_t mate = 0;

    bool isPonder = false;
    bool isInfinite = false;

    TimeInterval getThinkingTime() const {
        if (movetime != TimeInterval::zero()) { return movetime; }

        auto moves = movestogo ? movestogo : 60;
        auto average = (time[My] + (moves-1)*inc[My]) / moves;

        return std::min(time[My], average);
    }

};

#endif
