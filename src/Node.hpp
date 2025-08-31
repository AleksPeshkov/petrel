#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"
#include "Repetitions.hpp"
#include "UciMove.hpp"

class NodeRoot;
class UciGoLimit;
class Node;

enum Bound { NoBound, LowerBound, UpperBound, Exact };

// 8 byte, always replace slot, so no age field, only one score, depth and bound flags
class TtSlot {
    enum { DataBits =  34 }; // total size of all data fields
    static constexpr u64_t HashMask = ~static_cast<u64_t>(0) ^ (::singleton<u64_t>(DataBits+1) - 1);

    struct PACKED DataSmall {
        unsigned  from :6;
        unsigned    to :6;
        signed   score :14;
        unsigned bound :2;
        unsigned draft :6;
        unsigned      z:64 - DataBits;
    };

    union {
        z_t zobrist;
        DataSmall s;
    };

public:
    TtSlot () { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }
    TtSlot (Z z, Move move, Score, Bound, Ply);
    TtSlot (Node* node, Bound b);
    bool operator == (Z z) const { return (zobrist & HashMask) == (z & HashMask); }
    operator Move () const { return { static_cast<Square::_t>(s.from), static_cast<Square::_t>(s.to) }; }
    Score score(Ply ply) const { return Score{s.score}.fromTt(ply); }
    operator Bound () const { return static_cast<Bound>(s.bound); }
    Ply draft() const { return {static_cast<Ply::_t>(s.draft)}; }
};

class Node : public PositionMoves {
protected:
    friend class TtSlot;

    Node* const parent;
    Node* const grandParent;

    NodeRoot& root;

    RepetitionMask repMask;

    Ply ply; //distance from root
    Ply draft = {0}; //remaining depth

    TtSlot* origin;
    TtSlot  ttSlot;
    bool isHit = false;

    Score score = NoScore;
    Score alpha = MinusInfinity;
    Score beta = PlusInfinity;

    Move childMove = {}; // last move made from this node
    Move killer1 = {}; // first killer move to try at child-child nodes
    Move killer2 = {}; // second killer move to try at child-child nodes
    bool canBeKiller = false; // only moves at after killer stage will update killers

    Node (Node* n);

    ReturnStatus searchMove(Move move);
    ReturnStatus searchMove(Square from, Square to) { return searchMove({from, to}); }
    ReturnStatus searchIfLegal(Move move) { return parent->isLegalMove(move) ? searchMove(move) : ReturnStatus::Continue; }
    ReturnStatus negamax(Node*);
    ReturnStatus betaCutoff();
    ReturnStatus updatePv();

    ReturnStatus search();
    ReturnStatus searchMoves();
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

    void makeMove(Move move);
    void updateKillerMove();
    void updateTtPv();

    Color colorToMove() const;
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    Node (NodeRoot& r);
    ReturnStatus searchRoot();
    Score evaluate() const;
};

#endif
