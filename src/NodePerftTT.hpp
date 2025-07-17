#ifndef NODE_PERFT_TT_HPP
#define NODE_PERFT_TT_HPP

#include "Node.hpp"

class NodePerftTT : public Node {
protected:
    NodePerftTT& parent;
    ply_t draft;
    node_count_t perft = 0;

public:
    NodePerftTT (const PositionMoves& p, SearchControl& c, ply_t d);
    NodePerftTT (NodePerftTT& n);

    NodeControl visit(Square from, Square to);
    NodeControl visitChildren() override;
};

#endif
