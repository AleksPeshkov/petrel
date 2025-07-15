#ifndef NODE_AB_PV_HPP
#define NODE_AB_PV_HPP

#include "NodeAb.hpp"
#include "Score.hpp"

class NodeAbPv : public NodeAb {
public:
    NodeAbPv (NodeAbPv& n, ply_t r = 1) : NodeAb(n, r) {}
    NodeAbPv (const PositionMoves& p, SearchControl& c, ply_t d) : NodeAb(p, c, d) {}

    NodeControl visit(Square from, Square to) override;
};

#endif
