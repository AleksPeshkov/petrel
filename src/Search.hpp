#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "SearchRoot.hpp"
#include "Score.hpp"
#include "UciMove.hpp"

class UciGoLimit;
class NodeAb;

enum Bound { NoBound, LowerBound, UpperBound, Exact };

class PACKED TtSlot {
    z_t     zobrist:24;
    Score::_t score:14;
    Bound     bound:2;
    Square::_t from:6;
    Square::_t   to:6;
    Ply::_t   draft_:6;

    static inline constexpr z_t zMask = ::singleton<z_t>(40) - 1;

public:
    TtSlot () {}
    TtSlot (NodeAb* node, Bound b);

    bool operator == (Z z) const { return zobrist == (z >> 40); }
    operator Move () const { return {from, to}; }
    operator Score () const { return {score}; }
    operator Bound () const { return bound; }
    Ply draft() const { return draft_; }
};

class SearchThread : public Runnable {
    SearchRoot& root;
    const UciGoLimit& limit;
public:
    SearchThread (SearchRoot& r, const UciGoLimit& l) : root{r}, limit{l} {}
    void run() override;
};

class NodeAb : public PositionMoves {
protected:
    friend class TtSlot;

    NodeAb* const parent;
    NodeAb* const grandParent;

    SearchRoot& root; /* thread local */

    RepetitionMask repMask;

    Ply ply = 0; //distance from root
    Ply draft = 0; //remaining depth

    TtSlot* origin;
    TtSlot  ttSlot;
    bool isHit = false;

    Score score = NoScore;
    Score alpha = MinusInfinity;
    Score beta = PlusInfinity;

    MovesNumber movesMade = 0; // number of moves already made in this node

    Move childMove = {}; // last move made from this node
    Move killer1 = {}; // first killer move to try at child-child nodes
    Move killer2 = {}; // second killer move to try at child-child nodes
    bool canBeKiller; // only moves at after killer stage will update killers

    NodeAb (NodeAb* n) : parent{n}, grandParent{n->parent}, root{n->root}, ply{n->ply + 1} {}

    ReturnStatus visitIfLegal(Move move) { if (parent->isLegalMove(move)) { return visit(move); } return ReturnStatus::Continue; }
    ReturnStatus visit(Move);
    ReturnStatus negamax(NodeAb*);

    ReturnStatus searchMoves();
    ReturnStatus quiescence();

    ReturnStatus goodCaptures(NodeAb*);
    ReturnStatus notBadCaptures(NodeAb*);
    ReturnStatus allCaptures(NodeAb*);

    ReturnStatus goodCaptures(NodeAb*, Square);
    ReturnStatus notBadCaptures(NodeAb*, Square);
    ReturnStatus allCaptures(NodeAb*, Square);

    ReturnStatus safePromotions(NodeAb*);
    ReturnStatus allPromotions(NodeAb*);

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
