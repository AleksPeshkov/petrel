#ifndef NODE_AB_HPP
#define NODE_AB_HPP

#include "Node.hpp"
#include "Score.hpp"

class NodeAb : public Node {
protected:
    NodeAb& parent; //virtual

    Ply ply = 0u; //distance from root
    Ply draft = 0; //remaining depth

    Score score = NoScore;
    Score alpha = MinusInfinity;
    Score beta = PlusInfinity;

    NodeAb (const PositionMoves& p, SearchControl& c) : Node{p, c}, parent{*this} {}

    Score staticEval();
    NodeControl visit(Move);
    NodeControl negamax(Score, Move);

    Move createFullMove(Move move) const { return createFullMove(move.from(), move.to()); }
    Move createFullMove(Square from, Square to) const;

public:
    NodeAb (NodeAb& n) : Node{n.control}, parent{n}, ply{n.ply + 1} {}
    NodeControl visitChildren() override;

};

#endif
