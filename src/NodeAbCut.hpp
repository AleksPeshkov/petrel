#ifndef NODE_AB_CUT_HPP
#define NODE_AB_CUT_HPP

#include "NodeAb.hpp"
#include "Score.hpp"

class NodeAbCut : public NodeAb {
public:
    NodeAbCut (NodeAb& n, ply_t r = 1) : NodeAb(n, r) {}

    NodeControl visit(Square from, Square to) override;
};

#endif
