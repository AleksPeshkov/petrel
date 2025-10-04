#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"
#include "Repetitions.hpp"
#include "UciMove.hpp"
#include "Uci.hpp"

class Node;

enum Bound { NoBound, FailLow, FailHigh, ExactScore = FailLow | FailHigh };

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
    TtSlot (const Node* node);
    bool operator == (Z z) const { return (zobrist & HashMask) == (z & HashMask); }
    operator Move () const { return Move{ static_cast<Square::_t>(s.from), static_cast<Square::_t>(s.to) }; }
    Score score(Ply ply) const { return Score{s.score}.fromTt(ply); }
    operator Bound () const { return static_cast<Bound>(s.bound); }
    Ply draft() const { return s.draft; }
};

class Uci;

class Node : public PositionMoves {
protected:
    friend class TtSlot;

    const Uci& root; // common search thread data
    const Node* const parent = nullptr; // previous (ply-1, opposite side to move) node or nullptr
    const Node* const grandParent = nullptr; // previous side to move node (ply-2) or nullptr

    RepetitionMask repMask; // mini-hash of all previous reversible positions zobrist keys

    Ply ply{0}; // distance from root (root is ply == 0)
    Ply draft{0}; // remaining depth to horizon

    TtSlot* origin; // pointer to the slot in TT
    TtSlot  ttSlot;
    bool isHit = false; // this node found in TT

    mutable Score alpha = MinusInfinity; // alpha-beta window lower margin
    Score beta = PlusInfinity; // alpha-beta window upper margin
    mutable Score score = NoScore; // best score found so far
    mutable Bound bound = FailLow; // FailLow is default unless have found Exact or FailHigh move later

    mutable Move currentMove = {}; // last move made from *this into *child
    PvMoves::Index pvIndex{0}; // start of subPV for the current ply

    /**
     * Killer heuristic
     */
    mutable Move killer1 = {}; // primary killer, updated by previous siblings
    mutable Move killer2 = {}; // secondary, backup when primary updated
    bool canBeKiller = false; // good captures should not waste killer slots

    Node (const Node* parent); // prepare empty child node

    // propagate child last move search result score
    [[nodiscard]] ReturnStatus negamax(Node* child) const;
    void failHigh() const;
    void updateKillerMove(Move) const;

    void updatePv(Node* child) const;
    void refreshTtPv();

    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    // promotions to queen, winning or equal captures, also uncertain by current SEE captures
    [[nodiscard]] ReturnStatus goodCaptures(Node*, const PiMask&);

    // remaining (bad) captures and all underpromotions
    [[nodiscard]] ReturnStatus badCaptures(Node*, const PiMask&);

    [[nodiscard]] ReturnStatus searchIfLegal(Move move) {
        return parent->isLegalMove(move) ? searchMove(move) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchMove(Move move);
    void makeMove(Square from, Square to);

    // convert internal move to be printable in UCI format
    UciMove uciMove(Move move) const;

    constexpr Color colorToMove() const;
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    Node (const PositionMoves&, const Uci&);
    ReturnStatus searchRoot();
};

#endif
