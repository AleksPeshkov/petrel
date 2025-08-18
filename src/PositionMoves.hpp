#ifndef POSITION_MOVES_HPP
#define POSITION_MOVES_HPP

#include "Position.hpp"
#include "PiBbMatrix.hpp"
#include "Zobrist.hpp"

class PositionMoves : public Position {
public:
    PiBbMatrix moves; // generated legal only moves

protected:
    Bb attackedSquares; //squares attacked by all opponent pieces
    bool inCheck;

private:
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
    void makeMove(Square, Square);
    void makeMove(PositionMoves* parent, Square from, Square to);
    void makeMoveNoZobrist(PositionMoves* parent, Square from, Square to);

    void makeMove(Move move) { return makeMove(move.from(), move.to()); }
    void makeMove(PositionMoves* parent, Move move) { return makeMove(parent, move.from(), move.to()); }

    void makeMoves();
    bool isLegalMove(Move move) const { return move && isLegalMove(move.from(), move.to()); }
    bool isLegalMove(Square from, Square to) const;
};

#endif
