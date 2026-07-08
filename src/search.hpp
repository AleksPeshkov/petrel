#ifndef NODE_HPP
#define NODE_HPP

#include "history.hpp"
#include "PositionMoves.hpp"

class TtSlot;

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

    Move currentMove{}; // last move made from *this into *child
    Move bestMove{}; // TtMove or best move found
    std::array<Move, 2> killers{}; // Killer heuristic

    PrincipalVariation::Index pvIndex{0}; // start of subPV for the current ply
    TtSlot* tt{nullptr}; // pointer to the slot in TT
    ZHash childZHash; // updated from parent or reset caused by currentMove

    void clearNode(); // prepare empty node
    void assertOk() const;

    [[nodiscard]] ReturnStatus negamax(Ply R = 1_ply); // search with child.depth = depth - R, then apply child search score
    [[nodiscard]] ReturnStatus search();
    [[nodiscard]] ReturnStatus quiescence();

    [[nodiscard]] ReturnStatus searchNullMove();
    [[nodiscard]] ReturnStatus searchMove(Move, Ply R = 1_ply);
    [[nodiscard]] ReturnStatus searchMove(Square from, Square to, Ply R, CanBeKiller _canBeKiller = CanBeKiller::Yes) {
        return searchMove(toMove(from, to, _canBeKiller), R);
    }

    [[nodiscard]] ReturnStatus searchIfPossible(Move move, Ply R = 1_ply) {
        return isPossibleMove(move) ? searchMove(move, R) : ReturnStatus::Continue;
    }

    [[nodiscard]] ReturnStatus goodCaptures(PiMask); // winning promotions to queen, winning or equal captures
    [[nodiscard]] ReturnStatus goodNonCaptures(Pi, Bb, Ply R);

    [[nodiscard]] ReturnStatus contMove(ContIndex::_t, Move);
    [[nodiscard]] ReturnStatus checkMove(Move);

    void childNullMove();
    void childMove(Square, Square);
    void saveHistory();
    void saveNode(); // write search result into TT
    constexpr Ply finalR(Ply) const;

    constexpr bool hasAncestor(Ply n) const { return ply >= n; }
    constexpr bool hasDescendant(Ply n) const { return ply <= Ply{Ply::last()} - n; }
    constexpr bool hasParent() const { return hasAncestor(1_ply); }
    constexpr bool hasGrandParent() const { return hasAncestor(2_ply); }

    constexpr auto& ancestor(Ply n) const { assert (hasAncestor(n)); return *(const_cast<Node*>(this) - +n); } // ply-n
    constexpr auto& descendant(Ply n) const { assert (hasDescendant(n)); return *(const_cast<Node*>(this) + +n); } // ply+n
    constexpr auto& parent() const { return ancestor(1_ply); } // previous (ply-1) opposite side to move node
    constexpr auto& grandParent() const { return ancestor(2_ply); } // previous side to move node (ply-2)
    constexpr auto& child() const { return descendant(1_ply); } // child (ply+1) node to make moves into

    constexpr Move counterMove() const;
    constexpr Move followupMove() const;
    constexpr Move followupMove2() const;

    constexpr bool isRoot() const { return ply == 0_ply; } // ply == 0
    constexpr bool isPv() const { return ply == pvPly; } // ply == pvPly
    constexpr bool isCutNode() const { return (+ply - +pvPly) & 1; } // odd (ply - pvPly)
    constexpr bool isAllNode() const { return !isPv() && !isCutNode(); } // even (plv - pvPly)
    constexpr Ply currentR() const { return parent().depth - depth; } // parent.depth - depth

    constexpr Color colorToMove() const; // current node side to move color
    bool isDrawMaterial() const;
    bool isRepetition() const;

#ifndef NDEBUG
    // defined in Uci.cpp
    COLD void assert_fail(const char* assertion, const char* file, unsigned int line, const char* function) const;
#endif

public:
    constexpr Node() = default;
    constexpr explicit Node (Ply _ply) : ply{_ply} {}
    ReturnStatus searchRoot(const PositionMoves&);
};

class Tt;

// update TT with latest PV (in case it have been overwritten)
void savePv(const PositionMoves&, const PrincipalVariation&, const Tt&);

#endif
