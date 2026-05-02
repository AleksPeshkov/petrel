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
    constexpr TtSlot () : v_{0} {}

    constexpr TtSlot (Z z,
        Score score,
        Ply ply,
        Bound bound,
        Ply draft,
        Square from,
        Square to,
        bool canBeKiller
    ) : v_{
        (z & ZMask)
        | pack<_t>(score.tt(ply), ShiftScore)
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
    const Node* const parent{nullptr}; // previous (ply-1) opposite side to move node or nullptr
    const Node* const grandParent{nullptr}; // previous side to move node (ply-2) or nullptr
    Node* child{nullptr}; // child node to make moves into, created in search()

    ZHash zHash{}; // mini-hash of all previous reversible positions zobrist keys
    TtSlot* tt{nullptr}; // pointer to the slot in TT
    bool ttHit{false}; // this node found in TT
    TtSlot ttSlot; //TODO: remove me

    Ply ply{0}; // distance from root (root is ply == 0)
    Ply pvPly{0}; // ply of nearest PV node, if pvPly == ply, this is PV node
    PrincipalVariation::Index pvIndex{0}; // start of subPV for the current ply
    Ply depth{0}; // remaining depth to horizon (should be set before search)

    Score eval{NoScore}; // static evaluation of the current position
    Score alpha{MateLoss}; // alpha-beta window lower margin
    Score beta{MateWin}; // alpha-beta window upper margin
    Score score{NoScore}; // best score found, alpha <= score < beta
    Bound bound{FailLow}; // default FailLow, until Exact or FailHigh move will be found later

    HistoryMove currentMove{}; // last move made from *this into *child
    mutable std::array<HistoryMove, 3> killer{}; // Killer heuristic, mutable to update from const* grandParent
    bool canBeKiller{false};  // good captures and check evasions should not waste killer slots

public:
    Node (const PositionMoves& _position, const Uci& _uci) : PositionMoves{_position}, root{_uci} {}
    ReturnStatus searchRoot();

protected:
    Node (Node* parent); // prepare empty child node
    void assertOk() const;

    [[nodiscard]] ReturnStatus negamax(Ply R = 1_ply); // search with child->depth = depth - R, then apply child search score
    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    [[nodiscard]] ReturnStatus searchNullMove();
    [[nodiscard]] ReturnStatus searchMove(Square, Square, Ply R = 1_ply);

    [[nodiscard]] ReturnStatus searchIfPossible(HistoryMove move, Ply R = 1_ply) {
        return isPossibleMove(move) ? searchMove(move.from(), move.to(), R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus searchIfPossible(Square from, Square to, Ply R = 1_ply) {
        return isPossibleMove(from, to) ? searchMove(from, to, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus goodCaptures(PiMask); // winning promotions to queen, winning or equal captures
    [[nodiscard]] ReturnStatus goodNonCaptures(Pi, Bb moves, Ply R);

    [[nodiscard]] ReturnStatus counterMove();
    [[nodiscard]] ReturnStatus followMove();
    [[nodiscard]] ReturnStatus updatePv();

    void childNullMove();
    void childMove(Square, Square);
    void failHigh();
    void updateHistory(HistoryMove) const;

    constexpr Color colorToMove() const; // current node's side to move color
    constexpr bool isRoot() const { assert (parent == nullptr || ply > 0_ply); return parent == nullptr; } // ply == 0
    constexpr bool isPv() const { return ply == pvPly; } // ply == pvPly
    constexpr bool isCutNode() const { return (+ply - +pvPly) & 1; } // odd (ply - pvPly)
    constexpr bool isAllNode() const { return !isPv() && !isCutNode(); } // even (plv - pvPly)
    constexpr Ply currentR() const { assert (!isRoot()); return parent->depth - depth; } // parent->depth - depth
    Ply finalR(Ply) const;

    bool isDrawMaterial() const;
    bool isRepetition() const;

    void setTtMove() {
        if (ttHit && ttSlot.hasMove()) {
            assert (MY.has(ttSlot.from()));
            currentMove = historyMove(ttSlot.from(), ttSlot.to());
            canBeKiller = ttSlot.canBeKiller();
        } else {
            currentMove = {};
            canBeKiller = false;
        }
        assert (currentMove.none() || isPseudoLegal(currentMove));
    }
};

class Tt;

// update TT with latest PV (in case it have been overwritten)
void refreshTtPv(const PositionMoves&, const PrincipalVariation&, const Tt&);

#endif
