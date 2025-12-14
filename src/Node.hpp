#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"
#include "Repetitions.hpp"
#include "UciMove.hpp"
#include "Uci.hpp"

class Node;

enum Bound : u8_t { NoBound, FailLow, FailHigh, ExactScore = FailLow | FailHigh };

// 8 byte, always replace slot, so no age field, only one score, depth and bound flags
class TtSlot {
    using _t = u64_t;
    enum {
        ScoreMask = Score::Mask,
        BoundMask = 3,
        SquareMask = Square::Mask,
        DraftMask = Ply::Mask,
        KillerMask = 1,
    };
    enum {
        ScoreShift = 0,
        BoundShift = ScoreShift + 14,
        FromShift = BoundShift + 2,
        ToShift = FromShift + 6,
        DraftShift = ToShift + 6,
        KillerShift = DraftShift + 6,
        TotalShift = KillerShift + 1, // total size of all data fields
    };
    static constexpr u64_t HashMask = U64(0xffff'ffff'ffff'ffff) << TotalShift;

    _t v;

public:
    TtSlot () { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }

    TtSlot (Z z, Move move, Score score, Bound bound, Ply draft, bool canBeKiller) : v{
        (z & HashMask)
        | (static_cast<_t>(static_cast<unsigned>(score) - NoScore) << ScoreShift) // convert to unsigned
        | (static_cast<_t>(bound) << BoundShift)
        | (static_cast<_t>(move.from()) << FromShift)
        | (static_cast<_t>(move.to()) << ToShift)
        | (static_cast<_t>(draft) << DraftShift)
        | (static_cast<_t>(canBeKiller) << KillerShift)
    } {}

    TtSlot (const Node* node);
    bool operator == (Z z) const { return (v & HashMask) == (z & HashMask); }

    Move move() const { return Move{
        static_cast<Square::_t>(v >> FromShift & SquareMask),
        static_cast<Square::_t>(v >> ToShift & SquareMask)
    };}

    Bound bound() const { return static_cast<Bound>(v >> BoundShift & BoundMask); }
    Ply draft() const { return Ply{static_cast<Ply::_t>(v >> DraftShift & DraftMask)}; }
    bool canBeKiller() const { return v >> KillerShift & KillerMask; }

    Score score(Ply ply) const {
        // convert unsigned to signed
        int score = (v >> ScoreShift & ScoreMask) + NoScore;
        return Score{static_cast<Score::_t>(score)}.fromTt(ply);
    }
};

class Uci;

class Node : public PositionMoves {
protected:
    friend class TtSlot;

    const Uci& root; // common search thread data
    const Node* const parent = nullptr; // previous (ply-1, opposite side to move) node or nullptr
    const Node* const grandParent = nullptr; // previous side to move node (ply-2) or nullptr

    RepetitionHash repetitionHash; // mini-hash of all previous reversible positions zobrist keys

    Ply ply{0}; // distance from root (root is ply == 0)
    Ply depth{0}; // remaining depth to horizon

    TtSlot* tt; // pointer to the slot in TT
    TtSlot  ttSlot;
    bool isHit = false; // this node found in TT
    Score eval; // static evaluation of the current position

    mutable Score alpha = MinusInfinity; // alpha-beta window lower margin
    Score beta = PlusInfinity; // alpha-beta window upper margin
    mutable Score score = NoScore; // best score found so far
    mutable Bound bound = FailLow; // FailLow is default unless have found Exact or FailHigh move later
    bool isPv = true; // alpha < beta-1, cannot use constexpr as alpha may change during search

    mutable Move currentMove = {}; // last move made from *this into *child
    PvMoves::Index pvIndex{0}; // start of subPV for the current ply

    /**
     * Killer heuristic
     */
    mutable Move killer1 = {}; // the most fresh killer, always replaced by sibling's fail high
    mutable Move killer2 = {}; // previous killer1
    mutable Move killer3 = {}; // dynamic killer candidate, write back by descendants
    bool canBeKiller = false;  // good captures should not waste killer slots

    Node (const Node* parent); // prepare empty child node

    // propagate child last move search result score
    [[nodiscard]] ReturnStatus negamax(Node* child, Ply R) const;
    void failHigh() const;
    void updateKillerMove(Move) const;

    void updatePv() const;
    void refreshTtPv();

    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    // promotions to queen, winning or equal captures, also uncertain by current SEE captures
    [[nodiscard]] ReturnStatus goodCaptures(Node*, PiMask);
    [[nodiscard]] ReturnStatus goodNonCaptures(Node*, Pi, Bb moves, Ply R);

    [[nodiscard]] ReturnStatus searchIfLegal(Move move, Ply R = 1) {
        return parent->isLegalMove(move) ? searchMove(move, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchMove(Move move, Ply R = 1);
    [[nodiscard]] ReturnStatus searchNullMove(Ply R);
    void makeMove(Square from, Square to);

    // convert internal move to be printable in UCI format
    UciMove uciMove(Move move) const;

    // current node's side to move color
    constexpr Color colorToMove() const;

    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    Node (const PositionMoves&, const Uci&);
    ReturnStatus searchRoot();
};

#endif
