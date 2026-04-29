#ifndef NODE_HPP
#define NODE_HPP

#include "history.hpp"
#include "PositionMoves.hpp"

class Node;

// 8 byte, always replace slot, so no age field, only one score, depth and bound flags
class TtSlot {
    enum shift_t {
        ScoreShift = 0,
        BoundShift = ScoreShift + Score::Bits,
        ToShift = BoundShift + 2,
        FromShift = ToShift + Square::Bits,
        KillerShift = FromShift + Square::Bits,
        MoveShift = BoundShift + 2,
        DraftShift = MoveShift + TtMove::Bits,
        TotalShift = DraftShift + Ply::Bits, // total size of all data fields
        ZSlotBits = 64 - TotalShift, // size of zobrist bitfield
    };

    using _t = u64_t;

#ifndef NDEBUG
    union {
        _t v_;
        struct __attribute__((packed)) {
            Score::_t score_ :Score::Bits;
            Bound bound_ :2;
            Square::_t to_ :Square::Bits;
            Square::_t from_ :Square::Bits;
            CanBeKiller killer_ :1;
            Ply::_t draft_ :Ply::Bits;
            Z::_t z_ :ZSlotBits;
        } u;
    };
    static_assert (sizeof(u) == sizeof(v_));
#else
    _t v_;
#endif

    static constexpr _t ZSlotMask{ U64(0xffff'ffff'ffff'ffff) << (TotalShift) };

    static constexpr _t v(_t field, shift_t shift) { return field << shift; }

    template <typename T = int>
    constexpr T get(shift_t shift, T mask) const { return static_cast<T>((v_ >> shift) & mask); }

public:
    constexpr TtSlot () : v_{0} {}

    constexpr TtSlot (Z z,
        Score _score,
        Ply _ply,
        Bound _bound,
        TtMove _ttMove,
        Ply _draft
    ) : v_{
        (z.v() & ZSlotMask)
        | v(_score.tt(_ply), ScoreShift)
        | v(_bound, BoundShift)
        | v(_ttMove.v(), MoveShift)
        | v(_draft.v(), DraftShift)
    } {
        static_assert (sizeof(TtSlot) == sizeof(u64_t));

        assert (score(_ply) == _score);
        assert (bound() == _bound);
        assert (ttMove() == _ttMove);
        assert (draft() == _draft);
    }

    TtSlot (const Node* node);
    constexpr bool operator == (Z z) const { return (v_ & ZSlotMask) == (z.v() & ZSlotMask); }

    constexpr Score score(Ply ply) const { return Score::fromTt(get(ScoreShift, Score::Mask), ply); }
    constexpr Bound bound() const { return get<Bound>(BoundShift, BoundMask); }
    constexpr TtMove ttMove() const { return TtMove{get<TtMove::_t>(MoveShift, TtMove::Mask)}; }
    constexpr Ply draft() const { return Ply{get<Ply::_t>(DraftShift, Ply::Mask)}; }
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

    TtSlot* tt; // pointer to the slot in TT
    TtSlot  ttSlot;
    bool ttHit{false}; // this node found in TT
    Score eval{NoScore}; // static evaluation of the current position

    mutable Score alpha; // alpha-beta window lower margin
    Score beta; // alpha-beta window upper margin
    Ply pvPly; // ply of nearest PV node, if pvPly == ply, this is PV node
    mutable Score score{NoScore}; // best score found so far
    mutable Bound bound{FailLow}; // FailLow is default unless have found Exact or FailHigh move later

    mutable HistoryMove currentMove = {}; // last move made from *this into *child
    mutable std::array<HistoryMove, 3> killer = {}; // Killer heuristic
    mutable PrincipalVariation::Index pvIndex; // start of subPV for the current ply

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

    [[nodiscard]] ReturnStatus searchNullMove(Ply R);
    [[nodiscard]] ReturnStatus searchMove(HistoryMove, Ply R = 1_ply);
    [[nodiscard]] ReturnStatus searchIfPossible(HistoryMove move, Ply R = 1_ply) {
        return parent->isPossibleMove(move) ? searchMove(move, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchMove(Square from, Square to, Ply R, CanBeKiller _canBeKiller) {
        return searchMove(parent->historyMove(from, to, _canBeKiller), R);
    }

    [[nodiscard]] ReturnStatus searchMove(Square from, Square to, Ply R = 1_ply) {
        return searchMove(from, to, R, !parent->inCheck() ? CanBeKiller::Yes : CanBeKiller::No);
    }

    constexpr Color colorToMove() const; // current node's side to move color
    constexpr bool isRoot() const { assert (parent == nullptr || ply > 0_ply); return parent == nullptr; } // ply == 0
    constexpr bool isPv() const { return ply == pvPly; } // ply == pvPly
    constexpr bool isCutNode() const { return (ply - pvPly).v() & 1; } // odd (ply - pvPly)
    constexpr bool isAllNode() const { return !isPv() && !isCutNode(); } // even (plv - pvPly)
    constexpr Ply depthR() const { assert (!isRoot()); return parent->depth - depth; } // parent->depth - depth
    Ply adjustDepthR(Ply) const;

    bool isDrawMaterial() const;
    bool isRepetition() const;

    void setCurrentTtMove() const {
        currentMove = ttHit && ttSlot.ttMove().any() ? historyMove(ttSlot.ttMove()) : HistoryMove{};
        assert (currentMove.none() || isPseudoLegal(currentMove));
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

class Tt;

// update TT with latest PV (in case it have been overwritten)
void refreshTtPv(const PositionMoves&, const PrincipalVariation&, const Tt&);

#endif
