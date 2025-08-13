#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"
#include "Repetitions.hpp"
#include "Score.hpp"
#include "UciMove.hpp"

class NodeRoot;
class UciGoLimit;
class Node;

enum Bound { Empty, LowerBound, UpperBound, Exact };

class PACKED TtSlot {
    z_t     zobrist:24;
    Score::_t score:14;
    Bound     bound:2;
    Square::_t from:6;
    Square::_t   to:6;
    Ply::_t  draft_:6;

    static inline constexpr z_t zMask = ::singleton<z_t>(40) - 1;

public:
    TtSlot () { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }
    TtSlot (Z z, Score s, Bound b, Move move, Ply d);
    TtSlot (Node* node, Bound b);
    bool operator == (Z z) const { return zobrist == (z >> 40); }
    operator Move () const { return {from, to}; }
    operator Score () const { return {score}; }
    operator Bound () const { return bound; }
    Ply draft() const { return draft_; }
};

class Node : public PositionMoves {
protected:
    friend class TtSlot;

    Node* const parent;
    Node* const grandParent;

    NodeRoot& root;

    RepetitionMask repMask;

    Ply ply; //distance from root
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
    bool canBeKiller = false; // only moves at after killer stage will update killers

    Node (Node* n);

    ReturnStatus searchMove(Move move);
    ReturnStatus searchMove(Square from, Square to) { return searchMove({from, to}); }
    ReturnStatus searchIfLegal(Move move) { return parent->isLegalMove(move) ? searchMove(move) : ReturnStatus::Continue; }
    ReturnStatus negamax(Node*);

    ReturnStatus search();
    ReturnStatus quiescence();

    ReturnStatus goodCaptures(Node*);
    ReturnStatus notBadCaptures(Node*);
    ReturnStatus allCaptures(Node*);

    ReturnStatus goodCaptures(Node*, Square);
    ReturnStatus notBadCaptures(Node*, Square);
    ReturnStatus allCaptures(Node*, Square);

    ReturnStatus safePromotions(Node*);
    ReturnStatus allPromotions(Node*);

    UciMove uciMove(Move move) const { return uciMove(move.from(), move.to()); }
    UciMove uciMove(Square from, Square to) const;

    void updateKillerMove();
    void refreshPvInTt(Ply d);

    Color colorToMove() const;
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    Node (NodeRoot& r);
    ReturnStatus searchRoot();
    Score evaluate() const;
};

#endif
