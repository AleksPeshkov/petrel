#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "PositionMoves.hpp"

class NodeRoot;

class NodePerft : public PositionMoves {
    NodePerft* const parent;
    NodeRoot& root;

    node_count_t perft = 0;
    Ply draft;

    NodePerft (NodePerft* n) : parent{n}, root{n->root}, draft{n->draft-1} {}
    ReturnStatus visit();
    ReturnStatus visitMove(Square from, Square to);

public:
    NodePerft (NodeRoot&, Ply);
    ReturnStatus visitRoot();
};

#endif
