#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "PositionMoves.hpp"

class NodeRoot;

class NodePerft : public PositionMoves {
    NodeRoot& root;
    NodePerft* const parent;
    Ply draft;
    node_count_t perft = 0;

    NodePerft (NodePerft* n) : root{n->root}, parent{n}, draft{n->draft-1} {}

    ReturnStatus visit(Square from, Square to);
    ReturnStatus searchMoves();
    ReturnStatus searchDivide();

public:
    NodePerft (NodeRoot& r, Ply d);
    void visitRoot(bool isDivide);
};

#endif
