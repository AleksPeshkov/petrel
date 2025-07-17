#ifndef SEARCH_LIMIT_HPP
#define SEARCH_LIMIT_HPP

#include "typedefs.hpp"
#include "Duration.hpp"
#include "PositionFen.hpp"

class SearchLimit {
public:
    PositionFen positionMoves;

    Side::arrayOf<Duration> time = {{ Duration::zero(), Duration::zero() }};
    Side::arrayOf<Duration> inc = {{ Duration::zero(), Duration::zero() }};

    Duration movetime = Duration::zero();

    node_count_t nodes = NodeCountMax;
    index_t movestogo = 0;
    ply_t depth = 0;
    index_t mate = 0;

    bool isPonder = false;
    bool isInfinite = false;

    Duration getThinkingTime() const {
        if (movetime != Duration::zero()) { return movetime; }

        auto moves = movestogo ? movestogo : 60;
        auto average = (time[My] + (moves-1)*inc[My]) / moves;

        return std::min(time[My], average);
    }

};

#endif
