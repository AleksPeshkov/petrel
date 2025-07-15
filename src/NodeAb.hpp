#ifndef NODE_AB_HPP
#define NODE_AB_HPP

#include "Node.hpp"
#include "Score.hpp"

class NodeAb : public Node {
public:
    Score alpha = Score::Minimum;
    Score beta = Score::Maximum;
    Score bestScore = Score::Minimum;

public:
    NodeAb (NodeAb& n, ply_t r = 1) : Node(n, r), alpha{-n.beta}, beta{-n.alpha} {}
    NodeAb (const PositionMoves& p, SearchControl& c, ply_t d) : Node(p, c, d) {}

    NodeControl visit(Square from, Square to) override;
};

#endif
