#ifndef NODE_HPP
#define NODE_HPP

#include "history.hpp"
#include "PositionMoves.hpp"

class Node;

enum Bound : u8_t { NoBound,
    FailLow = 0b01, // upper bound
    FailHigh = 0b10, // lower bound
    ExactScore = FailLow | FailHigh
};

// 8 byte, always replace slot, so no age field, only one score, depth and bound flags
class TtSlot {
    using _t = u64_t;
    enum {
        ScoreShift = 0,
        BoundShift = ScoreShift + 14,
        ToShift = BoundShift + 2,
        FromShift = ToShift + 6,
        DraftShift = FromShift + 6,
        KillerShift = DraftShift + 6,
        TotalShift = KillerShift + 1, // total size of all data fields
    };
    static constexpr u64_t HashMask = U64(0xffff'ffff'ffff'ffff) << TotalShift;

    _t v;

public:
    TtSlot () { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }

    TtSlot (Z z,
        Score score = Score{NoScore},
        Ply ply = 0,
        Bound bound = NoBound,
        Ply draft = 0,
        Square from = Square{static_cast<Square::_t>(0)},
        Square to = Square{static_cast<Square::_t>(0)},
        bool canBeKiller = false
    ) : v{
        (z & HashMask)
        | (static_cast<_t>(score.toTt(ply)) << ScoreShift)
        | (static_cast<_t>(bound) << BoundShift)
        | (static_cast<_t>(from) << FromShift)
        | (static_cast<_t>(to) << ToShift)
        | (static_cast<_t>(draft) << DraftShift)
        | (static_cast<_t>(canBeKiller) << KillerShift)
    } {}

    TtSlot (const Node* node);
    bool operator == (Z z) const { return (v & HashMask) == (z & HashMask); }

    bool hasMove() const { return !(from() == 0 && to() == 0); }
    Square from() const { return Square{static_cast<Square::_t>(v >> FromShift & Square::Mask)}; }
    Square to() const { return Square{static_cast<Square::_t>(v >> ToShift & Square::Mask)}; }

    Bound bound() const { return static_cast<Bound>(v >> BoundShift & 0b11); }
    Ply draft() const { return Ply{static_cast<Ply::_t>(v >> DraftShift & Ply::Mask)}; }
    bool canBeKiller() const { return v >> KillerShift & 1; }

    Score score(Ply ply) const { return Score::fromTt(v >> ScoreShift & Score::Mask, ply); }
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
    Score eval{NoScore}; // static evaluation of the current position

    mutable Score alpha{MinusInfinity}; // alpha-beta window lower margin
    Score beta{PlusInfinity}; // alpha-beta window upper margin
    mutable Score score{NoScore}; // best score found so far
    mutable Bound bound = FailLow; // FailLow is default unless have found Exact or FailHigh move later
    bool isPv = true; // alpha < beta-1, cannot use constexpr as alpha may change during search

    mutable HistoryMove currentMove = {}; // last move made from *this into *child
    PvMoves::Index pvIndex{0}; // start of subPV for the current ply

    // Killer heuristic
    using KillerIndex = ::Index<3>;
    mutable std::array<HistoryMove, KillerIndex::Size> killer = {};
    bool canBeKiller = false;  // good captures should not waste killer slots

    Node (const Node* parent); // prepare empty child node

    // propagate child last move search result score
    [[nodiscard]] ReturnStatus negamax(Node* child, Ply R) const;
    void failHigh() const;
    void updateHistory(HistoryMove) const;

    void updatePv() const;

    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    // promotions to queen, winning or equal captures, plus complex SEE captures
    [[nodiscard]] ReturnStatus goodCaptures(Node*, PiMask);
    [[nodiscard]] ReturnStatus goodNonCaptures(Node*, Pi, Bb moves, Ply R);

    [[nodiscard]] ReturnStatus counterMove(Node*);
    [[nodiscard]] ReturnStatus followMove(Node*);

    [[nodiscard]] ReturnStatus searchMove(Pi, Square, Ply R = 1);
    [[nodiscard]] ReturnStatus searchMove(Square, Square, Ply R = 1);
    [[nodiscard]] ReturnStatus searchMove(HistoryMove move, Ply R = 1) { return searchMove(move.from(), move.to(), R); }
    [[nodiscard]] ReturnStatus searchNullMove(Ply R);

    [[nodiscard]] ReturnStatus searchIfPossible(Square from, Square to, Ply R = 1) {
        return parent->isPossibleMove(from, to) ? searchMove(from, to, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchIfPossible(HistoryMove move, Ply R = 1) {
        return parent->isPossibleMove(move) ? searchMove(move, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchIfPossible(Pi pi, Square to, Ply R = 1) {
        return parent->isPossibleMove(pi, to) ? searchMove(pi, to, R) : ReturnStatus::Continue;
    }

    void makeMove(Square from, Square to);

    // current node's side to move color
    constexpr Color colorToMove() const;

    bool isDrawMaterial() const;
    bool isRepetition() const;

    HistoryMove ttMove() const {
        return isHit && ttSlot.from() != ttSlot.to() && MY.has(ttSlot.from())
            ? HistoryMove{MY.typeAt(ttSlot.from()), ttSlot.from(), ttSlot.to()}
            : HistoryMove{}
        ;
    }

public:
    Node (const PositionMoves&, const Uci&);
    ReturnStatus searchRoot();
};

#endif
