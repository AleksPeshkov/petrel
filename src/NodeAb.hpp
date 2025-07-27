#ifndef NODE_AB_HPP
#define NODE_AB_HPP

#include "Node.hpp"
#include "Score.hpp"
#include "UciMove.hpp"

class NodeAb : public Node {
public:
    NodeAb* const parent;
    NodeAb* const grandParent;

    Ply ply = 0; //distance from root
    Ply draft = 0; //remaining depth

    Score score = NoScore;
    Score alpha = MinusInfinity;
    Score beta = PlusInfinity;

    UciMove currentMove = {};
    MovesNumber movesMade = 0; // number of moves already made in this node

    NodeAb (NodeAb* n) : Node{n->root}, parent{n}, grandParent{n->parent}, ply{n->ply + 1} {}
    NodeAb (const PositionMoves& p, SearchRoot& r) : Node{p, r}, parent{nullptr}, grandParent{nullptr} {}

    NodeControl visitIfLegal(Move move) { if (parent->isLegalMove(move)) { return visit(move); } return NodeControl::Continue; }
    NodeControl visit(Move);
    NodeControl negamax(Score);

    NodeControl visitChildren() override;
    NodeControl quiescence();

    NodeControl goodCaptures(NodeAb*);

    UciMove uciMove(Move move) const { return uciMove(move.from(), move.to()); }
    UciMove uciMove(Square from, Square to) const;

    Color colorToMove() const;
    Score evaluate();
};

#endif
