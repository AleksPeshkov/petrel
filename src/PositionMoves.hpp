#ifndef POSITION_MOVES_HPP
#define POSITION_MOVES_HPP

#include "Position.hpp"
#include "PiBbMatrix.hpp"

using MovesNumber = int; // number of (legal) moves in the position

class PositionMoves : public Position {
    mutable PiBbMatrix moves_; // generated strictly legal moves

    Bb bbAttacked_; // bitboard of squares attacked by any opponent (not side to move) piece (set during moves generation)
    mutable MovesNumber movesMade_; // number of moves already made in this node (set to 0 during moves generation)
    bool inCheck_; // king of current side to move is under attack (set during moves generation)

    //legal move generation helpers
    template <Side::_t> void excludePinnedMoves(PiMask);
    template <Side::_t> void correctCheckEvasionsByPawns(Bb, Square);
    template <Side::_t> void populateUnderpromotions();
    template <Side::_t> void generateEnPassantMoves();
    template <Side::_t> void generatePawnMoves();
    template <Side::_t> void generateCastlingMoves();
    template <Side::_t> void generateLegalKingMoves();
    template <Side::_t> void generateCheckEvasions();
    template <Side::_t> void generateMoves();

protected:
    void setMoves(const PiBbMatrix& moves) { moves_ = moves; movesMade_ = 0; }
    void clearMove(Square from, Square to) const { moves_.clear(MY.pi(from), to); ++movesMade_; }

    void makeMoveNoZobrist(const PositionMoves* parent, Square from, Square to);

public:
    void generateMoves();

    // not yet made set of legal moves
    constexpr const PiBbMatrix& moves() const { return moves_; }

    // count of already made legal (non-null) moves
    constexpr MovesNumber movesMade() const { return movesMade_; }

    constexpr HistoryType historyType(Square from, Square to) const {
        // any pawn move or castling or null move
        //TRICK: from == to can be either null move or rook underpromotion
        if (from == to || MY.isPawn(from) || MY.isKing(to)) { return HistoryType{HistoryPawn}; }

        constexpr HistoryType::_t fromPieceType[] = { HistoryQN, HistoryRB, HistoryRB, HistoryQN, HistoryPawn, HistoryKing };
        return HistoryType{fromPieceType[MY.typeAt(from).v()]};
    }

    HistoryMove historyMove(TtMove ttMove) const {
        return HistoryMove{ttMove, historyType(ttMove.from(), ttMove.to())};
    }

    HistoryMove historyMove(Square from, Square to, CanBeKiller _canBeKiller) const {
        return historyMove(TtMove{from, to, _canBeKiller});
    }

    // move is legal and not yet made
    constexpr bool isPossibleMove(Square from, Square to) const {
        return MY.has(from) && moves_.has(MY.pi(from), to);
    }

    // move is legal and not yet made
    constexpr bool isPossibleMove(HistoryMove move) const {
        if (move.none()) { return false; }

        Square from{move.from()};
        Square to{move.to()};

        return isPossibleMove(from, to) && move.historyType() == historyType(from, to);
    }

    constexpr bool isPseudoLegal(HistoryMove move) const {
        if (move.none()) { return false; }

        Square from{move.from()};
        Square to{move.to()};

        return MY.has(from) && move.historyType() == historyType(from, to);
    }

    // non capture nor promotion move
    bool isNonCapture(Pi, Square) const;

    // attacked squares by not side to move pieces (op)
    constexpr Bb bbAttacked() const { return bbAttacked_; }

    // side to move king is under attack
    constexpr bool inCheck() const { return inCheck_; }

    // not yet made legal moves of the target piece
    constexpr Bb bbMovesOf(Pi pi) const { return moves_.bb(pi); }

    // pieces that have a not yet made legal move to the target square
    PiMask canMoveTo(Square sq) const { return moves_.piMask(sq); }
};

#endif
