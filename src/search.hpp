#ifndef NODE_HPP
#define NODE_HPP

#include "history.hpp"
#include "PositionMoves.hpp"

class Node;

// 8 byte, always replace slot, so no age field, only one score, depth and bound flags
class TtSlot {
    enum {
        ShiftScore = 0,
        ShiftBound = ShiftScore + Score::bit_width(),
        ShiftDraft = ShiftBound + 2,
        ShiftTo = ShiftDraft + Ply::bit_width(),
        ShiftFrom = ShiftTo + Square::bit_width(),
        ShiftKiller = ShiftFrom + Square::bit_width(),
        TotalBits = ShiftKiller + 1, // total size of all data fields
        ZBits = 64 - TotalBits, // size of zobrist bitfield
    };

    using _t = u64_t;
    _t v_;
    static constexpr _t ZMask{ U64(0xffff'ffff'ffff'ffff) << (64 - ZBits) };

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
        (z & ZMask)
        | pack<_t>(score.toTt(ply), ShiftScore)
        | pack<_t>(bound, ShiftBound)
        | draft.pack<_t>(ShiftDraft)
        | from.pack<_t>(ShiftFrom)
        | to.pack<_t>(ShiftTo)
        | pack<_t>(canBeKiller, ShiftKiller)
    } { static_assert (sizeof(TtSlot) == sizeof(u64_t)); }

    TtSlot (const Node* node);
    constexpr bool operator == (Z z) const { return (v_ & ZMask) == (z & ZMask); }

    constexpr Score score(Ply ply) const { return Score::fromTt(::unpack(v_, ShiftScore, Score::mask()), ply); }
    constexpr Bound bound() const { return ::unpack(v_, ShiftBound, BoundMask); }
    constexpr Ply draft() const { return Ply::unpack(v_, ShiftDraft); }

    constexpr bool hasMove() const { return !(+from() == 0 && +to() == 0); }
    constexpr Square from() const { return Square::unpack(v_, ShiftFrom); }
    constexpr Square to() const { return Square::unpack(v_, ShiftTo); }
    constexpr bool canBeKiller() const { return ::unpack(v_, ShiftKiller, true); }
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
    constexpr bool isCutNode() const { return (+ply - +plyPv) & 1; } // odd (ply - plyPv)
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
