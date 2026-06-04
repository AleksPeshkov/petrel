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
        ShiftMove = ShiftTo,
        TotalBits = ShiftKiller + 1, // total size of all data fields
        ZBits = 64 - TotalBits, // size of zobrist bitfield
    };

    using _t = u64_t;

#ifndef NDEBUG
    union {
        _t v_;
        struct PACKED {
            Score::_t score_ :Score::bit_width();
            Bound bound_ : 2;
            Ply::_t draft_ : Ply::bit_width();
            Square::_t to_ : Square::bit_width();
            Square::_t from_ : Square::bit_width();
            CanBeKiller killer_ : 1;
            Z::_t z_ : ZBits;
        } u;
    };
    static_assert (sizeof(u) == sizeof(v_));
#else
    _t v_;
#endif

    static constexpr _t ZMask{ U64(0xffff'ffff'ffff'ffff) << (64 - ZBits) };

public:
    constexpr TtSlot () : v_{0} {}

    constexpr TtSlot (Z z,
        Score _score,
        Ply _ply,
        Bound _bound,
        Ply _draft,
        TtMove _ttMove
    ) : v_{
        (z & ZMask)
        | pack<_t>(_score.tt(_ply), ShiftScore)
        | pack<_t>(_bound, ShiftBound)
        | _draft.pack<_t>(ShiftDraft)
        | pack<_t>(*_ttMove, ShiftMove)
    } {
        static_assert (sizeof(TtSlot) == sizeof(u64_t));

        assert (score(_ply) == _score);
        assert (bound() == _bound);
        assert (draft() == _draft);
        assert (ttMove() == _ttMove);
    }

    constexpr bool operator == (Z z) const { return (v_ & ZMask) == (z & ZMask); }

    constexpr Score score(Ply ply) const { return Score::fromTt(::unpack(v_, ShiftScore, Score::mask()), ply); }
    constexpr Bound bound() const { return ::unpack(v_, ShiftBound, BoundMask); }
    constexpr Ply draft() const { return Ply::unpack(v_, ShiftDraft); }
    constexpr TtMove ttMove() const { return TtMove{::unpack(v_, ShiftMove, TtMove::mask())}; }
};

class Uci;

class Node : public PositionMoves {
protected:
    const Ply ply{0}; // distance from root (root is ply == 0)
    Ply pvPly{0}; // ply of nearest PV node, if pvPly == ply, this is PV node
    Ply depth{0}; // remaining depth to horizon (should be set before search)
    Ply baseR{0}; // base R (depth reduction), same for all child moves

    Score eval{NoScore}; // static evaluation of the current position
    Score alpha{MateLoss}; // alpha-beta window lower margin
    Score beta{MateWin}; // alpha-beta window upper margin
    Score score{NoScore}; // best score found, alpha <= score < beta
    Bound bound{FailLow}; // default FailLow, until Exact or FailHigh move will be found later

    HistoryMove currentMove{}; // last move made from *this into *child
    HistoryMove bestMove{}; // TtMove or best move found
    std::array<HistoryMove, 2> killer{}; // Killer heuristic

    PrincipalVariation::Index pvIndex{0}; // start of subPV for the current ply
    ZHash zHash{}; // mini-hash of all previous reversible positions zobrist keys
    TtSlot* tt{nullptr}; // pointer to the slot in TT
    bool ttHit{false}; // this node found in TT

    void prepareChild(); // prepare empty child node
    void assertOk() const;

    [[nodiscard]] ReturnStatus negamax(Ply R = 1_ply); // search with child->depth = depth - R, then apply child search score
    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    [[nodiscard]] ReturnStatus searchNullMove();
    [[nodiscard]] ReturnStatus searchMove(HistoryMove, Ply R = 1_ply);
    [[nodiscard]] ReturnStatus searchMove(Square from, Square to, Ply R, CanBeKiller _canBeKiller = CanBeKiller::Yes) {
        return searchMove(historyMove(from, to, _canBeKiller), R);
    }

    [[nodiscard]] ReturnStatus searchIfPossible(HistoryMove move, Ply R = 1_ply) {
        return isPossibleMove(move) ? searchMove(move, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus goodCaptures(PiMask); // winning promotions to queen, winning or equal captures
    [[nodiscard]] ReturnStatus goodNonCaptures(Pi, Bb moves, Ply R);

    [[nodiscard]] ReturnStatus contMove(ContIndex::_t, HistoryMove);

    void childNullMove();
    void childMove(Square, Square);
    void updateHistory();
    void saveNode(); // write search result into TT
    Ply finalR(Ply) const;

    constexpr bool hasParent() const { return ply >= 1_ply; }
    constexpr bool hasGrandParent() const { return ply >= 2_ply; }
    constexpr bool hasChild() const { return ply <= Ply{Ply::last()}; }
    constexpr bool hasAncestor(Ply _ply) const { return ply >= _ply; }

    constexpr Node* parent() const { assert (hasParent()); return const_cast<Node*>(this - 1); } // previous (ply-1) opposite side to move node
    constexpr Node* grandParent() const { assert (hasGrandParent()); return const_cast<Node*>(this - 2); } // previous side to move node (ply-2)
    constexpr Node* child() const { assert (hasChild()); return const_cast<Node*>(this + 1); } // child node to make moves into
    constexpr Node* ancestor(Ply _ply) const { assert (hasAncestor(_ply)); return const_cast<Node*>(this - +_ply); }

    constexpr bool isRoot() const { return ply == 0_ply; } // ply == 0
    constexpr bool isPv() const { return ply == pvPly; } // ply == pvPly
    constexpr bool isCutNode() const { return (+ply - +pvPly) & 1; } // odd (ply - pvPly)
    constexpr bool isAllNode() const { return !isPv() && !isCutNode(); } // even (plv - pvPly)
    constexpr Ply currentR() const { return parent()->depth - depth; } // parent->depth - depth

    constexpr Color colorToMove() const; // current node side to move color
    bool isDrawMaterial() const;
    bool isRepetition() const;

public:
    constexpr Node() = default;
    constexpr explicit Node (Ply _ply) : ply{_ply} {}
    ReturnStatus searchRoot(const PositionMoves&);
};

class Tt;

// update TT with latest PV (in case it have been overwritten)
void savePv(const PositionMoves&, const PrincipalVariation&, const Tt&);

#endif
