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
    enum {
        ScoreShift = 0,
        BoundShift = ScoreShift + 14,
        ToShift = BoundShift + 2,
        FromShift = ToShift + 6,
        DraftShift = FromShift + 6,
        KillerShift = DraftShift + 6,
        TotalShift = KillerShift + 1, // total size of all data fields
        ZobristBits = 64 - TotalShift, // size of zobrist bitfield
    };

    using _t = u64_t;
    _t v_;
    static constexpr _t HashMask{ U64(0xffff'ffff'ffff'ffff) << (64 - ZobristBits) };

public:
    constexpr TtSlot (Z z = {},
        Score score = Score{NoScore},
        Ply ply = 0_ply,
        Bound bound = NoBound,
        Ply draft = 0_ply,
        Square from = Square{static_cast<Square::_t>(0)},
        Square to = Square{static_cast<Square::_t>(0)},
        bool canBeKiller = false
    ) : v_{
        (z.v() & HashMask)
        | (static_cast<_t>(score.toTt(ply)) << ScoreShift)
        | (static_cast<_t>(bound) << BoundShift)
        | (static_cast<_t>(from.v()) << FromShift)
        | (static_cast<_t>(to.v()) << ToShift)
        | (static_cast<_t>(draft.v()) << DraftShift)
        | (static_cast<_t>(canBeKiller) << KillerShift)
    } { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }

    TtSlot (const Node* node);
    constexpr bool operator == (Z z) const { return (v_ & HashMask) == (z.v() & HashMask) && v_ != 0; }

    bool hasMove() const { return !(from().v() == 0 && to().v() == 0); }
    Square from() const { return Square{static_cast<Square::_t>(v_ >> FromShift & Square::Mask)}; }
    Square to() const { return Square{static_cast<Square::_t>(v_ >> ToShift & Square::Mask)}; }

    Bound bound() const { return static_cast<Bound>(v_ >> BoundShift & 0b11); }
    Ply draft() const { return Ply{static_cast<Ply::_t>(v_ >> DraftShift & Ply::Mask)}; }
    bool canBeKiller() const { return v_ >> KillerShift & 1; }

    Score score(Ply ply) const { return Score::fromTt(v_ >> ScoreShift & Score::Mask, ply); }
};

class Uci;

class Node : public PositionMoves {
protected:
    friend class TtSlot;

    const Uci& root; // common search thread data
    const Node* const parent; // previous (ply-1) opposite side to move node or nullptr
    const Node* const grandParent; // previous side to move node (ply-2) or nullptr
    Node* child = nullptr; // child node to make moves into, created in search()

    RepHash repHash; // mini-hash of all previous reversible positions zobrist keys

    Ply ply; // distance from root (root is ply == 0)
    Ply depth{0}; // remaining depth to horizon (should be set before search)
    Ply plyPv; // ply of nearest PV node, if plyPv == ply, this is PV node

    TtSlot* tt; // pointer to the slot in TT
    TtSlot  ttSlot;
    bool ttHit{false}; // this node found in TT
    Score eval{NoScore}; // static evaluation of the current position

    mutable Score alpha; // alpha-beta window lower margin
    Score beta; // alpha-beta window upper margin
    mutable Score score{NoScore}; // best score found so far
    mutable Bound bound{FailLow}; // FailLow is default unless have found Exact or FailHigh move later

    mutable HistoryMove currentMove = {}; // last move made from *this into *child
    PrincipalVariation::Index pvIndex; // start of subPV for the current ply

    // Killer heuristic
    mutable std::array<HistoryMove, 3> killer = {};
    bool canBeKiller{false};  // good captures and check evasions should not waste killer slots

    Node (const Node* parent); // prepare empty child node

    // propagate child last move search result score
    [[nodiscard]] ReturnStatus negamax(Ply R = 1_ply) const;
    void failHigh() const;
    void updateHistory(HistoryMove) const;

    [[nodiscard]] ReturnStatus updatePv() const;

    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    // promotions to queen, winning or equal captures, plus complex SEE captures
    [[nodiscard]] ReturnStatus goodCaptures(PiMask);
    [[nodiscard]] ReturnStatus goodNonCaptures(Pi, Bb moves, Ply R);

    [[nodiscard]] ReturnStatus counterMove();
    [[nodiscard]] ReturnStatus followMove();

    [[nodiscard]] ReturnStatus searchMove(Square, Square, Ply R = 1_ply);
    [[nodiscard]] ReturnStatus searchNullMove(Ply R);

    [[nodiscard]] ReturnStatus searchIfPossible(Square from, Square to, Ply R = 1_ply) {
        return parent->isPossibleMove(from, to) ? searchMove(from, to, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchIfPossible(HistoryMove move, Ply R = 1_ply) {
        return parent->isPossibleMove(move) ? searchMove(move.from(), move.to(), R) : ReturnStatus::Continue;
    }

    constexpr bool isRoot() const { assert (parent == nullptr || ply > 0_ply); return parent == nullptr; } // ply == 0
    constexpr bool isPv() const { return ply == plyPv; } // ply == plyPv
    constexpr bool isCutNode() const { return (ply - plyPv).v() & 1; } // odd (ply - plyPv)
    constexpr bool isAllNode() const { return !isPv() && !isCutNode(); } // even (plv - plyPv)
    constexpr Ply depthR() const { assert (!isRoot()); return parent->depth - depth; } // parent->depth - depth
    Ply adjustDepthR(Ply) const;

    // current node's side to move color
    constexpr Color colorToMove() const;

    bool isDrawMaterial() const;
    bool isRepetition() const;

    HistoryMove ttMove() const {
        return ttHit && ttSlot.from() != ttSlot.to() && MY.has(ttSlot.from())
            ? HistoryMove{MY.typeAt(ttSlot.from()), ttSlot.from(), ttSlot.to()}
            : HistoryMove{}
        ;
    }

public:
    void assertOk() const {
        assert (alpha < beta);
        if (score.any()) {
            assert (score < beta || bound == FailHigh);
            assert (alpha <= score || bound == FailLow);
        }
        assert (Score{MateLoss} <= alpha);
        assert (beta <= Score{MateWin});
    }

    Node (const PositionMoves&, const Uci&);
    ReturnStatus searchRoot();
};

#endif
