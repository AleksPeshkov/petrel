#ifndef NODE_PERFT_TT_HPP
#define NODE_PERFT_TT_HPP

#include "Node.hpp"

class NodePerftTT : public Node {
protected:
    NodePerftTT* const parent;
    Ply draft;
    node_count_t perft = 0;

public:
    NodePerftTT (const PositionMoves&, SearchRoot&, Ply);
    NodePerftTT (NodePerftTT*);

    NodeControl visit(Square from, Square to);
    NodeControl visitChildren() override;
};

#endif
