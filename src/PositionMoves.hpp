#ifndef POSITION_MOVES_HPP
#define POSITION_MOVES_HPP

#include "Position.hpp"
#include "Move.hpp"
#include "PiBb.hpp"
#include "Zobrist.hpp"

class PositionMoves : public Position {
protected:
    PiBb moves; //generated moves from My side

private:
    Bb attackedSquares; //squares attacked by all opponent pieces

    Zobrist zobrist{0};
    index_t _movesCount = 0;

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

    Zobrist createZobrist(Square, Square) const;
    Zobrist generateZobrist() const;

protected:
    void makeMove(Square, Square);
    void makeMove(PositionMoves& parent, Square, Square);
    void makeMove(Move move) { return makeMove(move.from(), move.to()); }
    void makeMove(PositionMoves& parent, Move move) { return makeMove(parent, move.from(), move.to()); }

    void generateMoves();
    bool isLegalMove(Move move) const { return move && isLegalMove(move.from(), move.to()); }
    bool isLegalMove(Square from, Square to) const;
    void clearMove(Move move);
    void clearMove(Pi pi, Square to);
    void setMoves(const PiBb& m, index_t n);

    bool inCheck() const;

    void setZobrist(const PositionMoves& parent, Square from, Square to) { zobrist = parent.createZobrist(from, to); }
    void setZobrist() { zobrist = generateZobrist(); }

    const Zobrist& getZobrist() const { return zobrist; }
    index_t movesCount() const { return _movesCount; }
    PiBb cloneMoves() const { return PiBb{moves}; }
};

#endif
