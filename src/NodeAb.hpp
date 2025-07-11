#ifndef NODE_AB_HPP
#define NODE_AB_HPP

#include "Node.hpp"
#include "Score.hpp"

class NodeAb : public Node {
protected:
    ply_t draft; //remaining depth
    ply_t ply = 0; //distance from root
    Score alpha = Score::Minimum;
    Score beta = Score::Maximum;
    Score score = Score::Minimum;

    NodeAb (const PositionMoves& p, SearchControl& c, ply_t d) : Node{p, c}, draft{d} {}

    NodeControl visit(Square from, Square to) override;

public:
    NodeAb (NodeAb& n, ply_t r = 1) : Node{n}, draft{n.draft - r}, ply{n.ply + 1}, alpha{-n.beta}, beta{-n.alpha} {}
    NodeControl visitChildren() override;

};

#endif
