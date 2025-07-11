#ifndef NODE_PERFT_TT_HPP
#define NODE_PERFT_TT_HPP

#include "NodePerft.hpp"

class NodePerftTT : public NodePerft {
    friend class NodePerft;
protected:
    ply_t draft;
    node_count_t perft = 0;

    void updateParentPerft() {
        assert (&parent != this);
        static_cast<NodePerftTT&>(parent).perft += perft;
        perft = 0;
    }

    NodePerftTT (const PositionMoves& p, SearchControl& c, ply_t d) : NodePerft{p, c}, draft{d} {}

public:
    NodePerftTT (NodePerftTT& n) : NodePerft{n}, draft{n.draft - 1} {}
    NodeControl visit(Square from, Square to) override;
};

#endif
