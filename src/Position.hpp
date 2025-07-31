#ifndef POSITION_HPP
#define POSITION_HPP

#include "PositionSide.hpp"
#include "Zobrist.hpp"

// side to move
#define MY ((*this)[My])

// opponent side
#define OP ((*this)[Op])

class Position {
    Side::arrayOf<PositionSide> positionSide; //copied from the parent, updated incrementally
    Side::arrayOf<Bb> occupiedBb; // both color pieces combined, recalculated after each move

protected:
    Zobrist zobrist; // incrementally updated position hash
    Square lastPieceTo; // last moved piece actual destination square (rook square in case of castling or en passant capture pawn square)

private:
    template <Side::_t> void updateSliderAttacks(PiMask);
    template <Side::_t> void updateSliderAttacks(PiMask, PiMask);

    enum UpdateZobrist { NoZobrist, WithZobrist };
    template <Side::_t, UpdateZobrist = WithZobrist> void makeMove(Square, Square);

    template <Side::_t> Zobrist generateZobrist() const;

    //calculate Zobrist key from scratch
    Zobrist createZobrist(Square, Square) const;
    Zobrist generateZobrist() const;

public:
    constexpr PositionSide& operator[] (Side side) { return positionSide[side]; }
    constexpr const PositionSide& operator[] (Side side) const { return positionSide[side]; }

    template <Side::_t My> constexpr const Bb& occupied() const { return occupiedBb[My]; }

    Score evaluate() const { return PositionSide::evaluate(MY, OP); }

    // 1st step of making a move: update the zobrist hash
    void makeZobrist(const Position* parent, Square from, Square to) { zobrist = parent->createZobrist(from, to); }

    // 2nd step of making a move: update the position (including attack matrix)
    void makeMoveNoZobrist(const Position*, Square, Square);

    // 1st and 2nd step combined
    void makeMove(const Position*, Square, Square);

    void makeMove(Square, Square);
    bool isSpecial(Square, Square) const;

    const Zobrist& getZobrist() const { return zobrist; }
    void setZobrist() { zobrist = generateZobrist(); }

    //initial position setup
    bool dropValid(Side, PieceType, Square);
    bool afterDrop();
    template <Side::_t> void setLegalEnPassant(Pi, Square);
};

#endif
