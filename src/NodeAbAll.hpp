#ifndef NODE_AB_ALL_HPP
#define NODE_AB_ALL_HPP

#include "NodeAb.hpp"
#include "Score.hpp"

class NodeAbAll : public NodeAb {
public:
    NodeAbAll (NodeAb& n, ply_t r = 1) : NodeAb(n, r) {}

    NodeControl visit(Square from, Square to) override;
};

#endif
