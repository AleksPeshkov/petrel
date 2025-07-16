#ifndef NODE_AB_HPP
#define NODE_AB_HPP

#include "Node.hpp"
#include "Score.hpp"

class NodeAb : public Node {
protected:
    NodeAb& parent; //virtual

    ply_t ply = 0; //distance from root
    ply_t draft = 0; //remaining depth

    Score score = Score::None;
    Score alpha = Score::Minimum;
    Score beta = Score::Maximum;

    NodeAb (const PositionMoves& p, SearchControl& c) : Node{p, c}, parent{*this} {}

    NodeControl visit(Move);
    NodeControl negamax(Score, Move);

public:
    NodeAb (NodeAb& n) : Node{n.control}, parent{n}, ply{n.ply + 1} {}
    NodeControl visitChildren() override;

};

#endif
