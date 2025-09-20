#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "PositionMoves.hpp"

class Uci;

class NodePerft : public PositionMoves {
    NodePerft* const parent;
    Uci& root;

    node_count_t perft = 0;
    Ply draft;

    NodePerft (NodePerft* n) : parent{n}, root{n->root}, draft{n->draft-1} {}
    ReturnStatus visit();
    ReturnStatus visitMove(Square from, Square to);

public:
    NodePerft (const PositionMoves&, Uci&, Ply);
    ReturnStatus visitRoot();
};

#endif
