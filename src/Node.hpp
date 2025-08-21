#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"
#include "Repetitions.hpp"
#include "UciMove.hpp"

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
        Z::_t zobrist;
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

class NodeRoot;

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

    [[nodiscard]] ReturnStatus searchMove(Move move);
    [[nodiscard]] ReturnStatus searchMove(Square from, Square to) { return searchMove({from, to}); }
    [[nodiscard]] ReturnStatus searchIfLegal(Move move) { return parent->isLegalMove(move) ? searchMove(move) : ReturnStatus::Continue; }
    [[nodiscard]] ReturnStatus negamax(Node*);
    [[nodiscard]] ReturnStatus betaCutoff();
    [[nodiscard]] ReturnStatus updatePv();

    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus searchMoves();
    [[nodiscard]] ReturnStatus quiescence();

    [[nodiscard]] ReturnStatus goodCaptures(Node*);
    [[nodiscard]] ReturnStatus notBadCaptures(Node*);
    [[nodiscard]] ReturnStatus allCaptures(Node*);

    [[nodiscard]] ReturnStatus goodCaptures(Node*, Square);
    [[nodiscard]] ReturnStatus notBadCaptures(Node*, Square);
    [[nodiscard]] ReturnStatus allCaptures(Node*, Square);

    [[nodiscard]] ReturnStatus safePromotions(Node*);
    [[nodiscard]] ReturnStatus allPromotions(Node*);

    UciMove uciMove(Move move) const { return uciMove(move.from(), move.to()); }
    UciMove uciMove(Square from, Square to) const;

    void makeMove(Move move);
    void updateKillerMove();
    void updateTtPv();

    constexpr Color colorToMove() const;
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    Node (NodeRoot& r);
    ReturnStatus searchRoot();
};

#endif
