#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "PositionMoves.hpp"

class Uci;

class NodePerft : public PositionMoves {
    NodePerft* const parent{nullptr};
    node_count_t perft = 0;
    Ply depth;

    NodePerft (NodePerft* n) : parent{n}, depth{n->depth - 1_ply} {}
    ReturnStatus visit();
    ReturnStatus visitMove(Square from, Square to);

public:
    NodePerft (const PositionMoves& pos, Ply d) : PositionMoves{pos}, depth{d} {}
    ReturnStatus visitRoot();
};

#endif
