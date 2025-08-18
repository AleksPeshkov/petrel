#ifndef POSITION_MOVES_HPP
#define POSITION_MOVES_HPP

#include "Position.hpp"
#include "PiBbMatrix.hpp"
#include "Zobrist.hpp"

class PositionMoves : public Position {
    PiBbMatrix moves_; // generated strictly legal moves

    Bb bbAttacked_; // bitboard of squares attacked by any opponent (not side to move) piece (set during moves generation)
    MovesNumber movesMade_; // number of moves already made in this node (set to 0 during moves generation)
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
    void clearMove(Square from, Square to) { moves_.clear(MY.pieceAt(from), to); ++movesMade_; }

    void makeMove(Square, Square);
    void makeMove(PositionMoves* parent, Square from, Square to);
    void makeMoveNoZobrist(PositionMoves* parent, Square from, Square to);

    void makeMove(Move move) { return makeMove(move.from(), move.to()); }
    void makeMove(PositionMoves* parent, Move move) { return makeMove(parent, move.from(), move.to()); }

    void generateMoves();

public:
    // strictly legal moves not yet made
    constexpr const PiBbMatrix& moves() const { return moves_; }

    // number of moves have been already made (and have already cleared from the moves matrix))
    constexpr const MovesNumber& movesMade() const { return movesMade_; }

    // check if the given move is legal and not yet made
    bool isLegalMove(Move move) const {
        return move && isLegalMove(move.from(), move.to());
    }

    // check if the given move is legal and not yet made
    bool isLegalMove(Square from, Square to) const {
        return MY.has(from) && moves_.has(MY.pieceAt(from), to);
    }

    // bitboard of squares attacked by any opponent (not side to move) piece (set during moves generation)
    constexpr const Bb& bbAttacked() const { return bbAttacked_; }

    // king of current side to move is under attack
    constexpr const bool& inCheck() const { return inCheck_; }

    // bitboard of all moves of the given piece not yet made
    constexpr Bb movesOf(Pi pi) const { return moves_[pi]; }

    // pieces that have a not yet made legal move to the given square
    PiMask canMoveTo(Square sq) const { return moves_[sq]; }
};

#endif
