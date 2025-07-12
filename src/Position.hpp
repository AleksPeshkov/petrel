#ifndef POSITION_HPP
#define POSITION_HPP

#include "PositionSide.hpp"
#include "Score.hpp"

class Position {
    Side::arrayOf<PositionSide> positionSide;

    template <Side::_t> void updateSliderAttacks(PiMask);
    template <Side::_t> void updateSliderAttacks(PiMask, PiMask);
    template <Side::_t> void playKingMove(Square, Square);
    template <Side::_t> void playPawnMove(Pi, Square, Square);
    template <Side::_t> void playCastling(Pi, Square, Square);
    template <Side::_t> void makeMove(Square, Square);

protected:
    constexpr PositionSide& operator[] (Side side) { return positionSide[side]; }

public:
    constexpr const PositionSide& operator[] (Side side) const { return positionSide[side]; }

    Score evaluate() const { return PositionSide::evaluate((*this)[My], (*this)[Op]); }

    void makeMove(const Position&, Square, Square);
    void makeMove(Square, Square);

    //initial position setup
    bool dropValid(Side, PieceType, Square);
    bool afterDrop();
    template <Side::_t> void setLegalEnPassant(Pi, Square);
};

#endif
