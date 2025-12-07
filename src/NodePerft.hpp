#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "PositionMoves.hpp"

class Uci;

class NodePerft : public PositionMoves {
    NodePerft* const parent;
    Uci& root;

    node_count_t perft = 0;
    Ply depth;

    NodePerft (NodePerft* n) : parent{n}, root{n->root}, depth{n->depth-1} {}
    ReturnStatus visit();
    ReturnStatus visitMove(Square from, Square to);

public:
    NodePerft (const PositionMoves&, Uci&, Ply);
    ReturnStatus visitRoot();
};

#endif
