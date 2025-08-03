#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "SearchRoot.hpp"
#include "Score.hpp"
#include "UciMove.hpp"

class UciGoLimit;

class SearchThread : public Runnable {
    SearchRoot& root;
    const UciGoLimit& limit;
public:
    SearchThread (SearchRoot& r, const UciGoLimit& l) : root{r}, limit{l} {}
    void run() override;
};

class NodeAb : public PositionMoves {
protected:
    NodeAb* const parent;
    NodeAb* const grandParent;

    SearchRoot& root; /* thread local */

    RepetitionMask repMask;

    Ply ply = 0; //distance from root
    Ply draft = 0; //remaining depth

    Score score = NoScore;
    Score alpha = MinusInfinity;
    Score beta = PlusInfinity;

    UciMove currentMove = {};
    MovesNumber movesMade = 0; // number of moves already made in this node

    Move killer1 = {}; // first killer move to try at child-child nodes
    Move killer2 = {}; // second killer move to try at child-child nodes
    bool canBeKiller; // only moves at after killer stage will update killers

    NodeAb (NodeAb* n) : parent{n}, grandParent{n->parent}, root{n->root}, ply{n->ply + 1} {}

    ReturnStatus visitIfLegal(Move move) { if (parent->isLegalMove(move)) { return visit(move); } return ReturnStatus::Continue; }
    ReturnStatus visit(Move);
    ReturnStatus negamax(Score);

    ReturnStatus searchMoves();
    ReturnStatus quiescence();

    ReturnStatus goodCaptures(NodeAb*);
    ReturnStatus goodCaptures(NodeAb*, Square);

    UciMove uciMove(Move move) const { return uciMove(move.from(), move.to()); }
    UciMove uciMove(Square from, Square to) const;

    void updateKillerMove();

    Color colorToMove() const;
    Score evaluate();
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    NodeAb (const PositionMoves& p, SearchRoot& r) : PositionMoves{p}, parent{nullptr}, grandParent{nullptr}, root{r} {}
    ReturnStatus visitRoot(Ply);
};

#endif
