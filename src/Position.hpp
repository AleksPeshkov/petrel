#ifndef POSITION_HPP
#define POSITION_HPP

#include "PositionSide.hpp"
#include "Zobrist.hpp"

// side to move
#define MY positionSide(My)

// opponent side
#define OP positionSide(Op)

// all occupied squares by both sides from the current side point of view
#define OCCUPIED occupied(My)

class Position {
    Side::arrayOf<PositionSide> positionSide_; //copied from the parent, updated incrementally
    Side::arrayOf<Bb> occupied_; // both color pieces combined, updated from positionSide[] after each move

    Zobrist zobrist_; // incrementally updated position hash
    Square movedPieceFrom_; // last moved piece (rook in case of castling) initial square
    Square movedPieceTo_; // last moved piece actual destination square (rook square in case of castling or en passant capture pawn square)
    Rule50 rule50_; // number of halfmoves since last capture or pawn move, incremented or reset by makeMove()

    template <Side::_t> void updateSliderAttacks(PiMask);
    template <Side::_t> void updateSliderAttacks(PiMask, PiMask);

    enum UpdateZobrist { NoZobrist, WithZobrist };
    template <Side::_t, UpdateZobrist = WithZobrist> void makeMove(Square, Square);

    template <Side::_t> Zobrist generateZobrist() const;

    // update Zobrist key incrementally
    Zobrist createZobrist(Square, Square) const;

    // calculate Zobrist key from scratch
    Zobrist generateZobrist() const;

protected:
    constexpr PositionSide& positionSide(Side::_t side) { return positionSide_[side]; }

    // update the zobrist hash of incoming move
    void makeZobrist(const Position* parent, Square from, Square to) { zobrist_ = parent->createZobrist(from, to); }

    // update the position without updating the zobrist hash
    void makeMoveNoZobrist(const Position*, Square, Square);

    // update the position and its zobrist hash
    void makeMove(const Position*, Square, Square);

    template <Side::_t> void setLegalEnPassant(Pi, Square);
    void setZobrist() { zobrist_ = generateZobrist(); }
    void setRule50(const Rule50& rule50) { rule50_ = rule50; }

public:
    constexpr const PositionSide& positionSide(Side::_t side) const { return positionSide_[side]; }

    // all occupied squares by both sides from the given side point of view
    constexpr const Bb& occupied(Side::_t side) const { return occupied_[side]; }

    // position hash
    constexpr const Zobrist& zobrist() const { return zobrist_; }

    // last moved piece from square (differs from last move.from() in case of special move)
    constexpr Square movedPieceFrom() const { return movedPieceFrom_; }

    // last moved piece to square (differs from last move.to() in case of special move)
    constexpr Square movedPieceTo() const { return movedPieceTo_; }

    // number of halfmoves since last capture or pawn move
    constexpr const Rule50& rule50() const { return rule50_; }

    Score evaluate() const { return Evaluation::evaluate(MY.evaluation(), OP.evaluation()); }

    bool isSpecial(Square, Square) const;

    // make move directly inside position itself
    void makeMove(Square, Square);

// initial position setup

    bool dropValid(Side, PieceType, Square);
    bool afterDrop();
};

#endif
