#ifndef POSITION_MOVES_HPP
#define POSITION_MOVES_HPP

#include "Position.hpp"
#include "PiBb.hpp"
#include "Zobrist.hpp"

class PositionMoves : public Position {
    PiBb moves; //generated moves from My side
    Bb attackedSquares; //squares attacked by all opponent pieces

    Zobrist zobrist{0};
    Score _staticEval = Score::None;
    index_t _movesCount = 0;

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

    Zobrist createZobrist(Square, Square) const;
    Zobrist generateZobrist() const;

protected:
    void playMove(Square, Square);
    void playMove(const Position&, Square, Square);

    void generateMoves();

    void clearMove(Pi pi, Square sq) { moves.clear(pi, sq); }
    void setMoves(const PiBb& m, index_t n) { moves = m; _movesCount = n; assert (moves.count() == n);  }

    void setZobrist(const PositionMoves& parent, Square from, Square to) { zobrist = parent.createZobrist(from, to); }
    void setZobrist() { zobrist = generateZobrist(); }

public:
    const Zobrist& getZobrist() const { return zobrist; }
    index_t movesCount() const { return _movesCount; }
    Score staticEval() const { return _staticEval; }
    PiBb cloneMoves() const { return PiBb{moves}; }

    bool isLegalMove(Square from, Square to) const;
};

#endif
