#ifndef POSITION_HPP
#define POSITION_HPP

#include "PositionSide.hpp"
#include "Zobrist.hpp"

// side to move
#define MY positionSide(Side{My})

// opponent side
#define OP positionSide(Side{Op})

// all occupied squares by both sides from the current side point of view
#define OCCUPIED occupied(Side{My})

// number of halfmoves without capture or pawn move
class Rule50 {
    int v;
    static constexpr int Draw = 100;

public:
    constexpr Rule50() : v{0} {}
    constexpr void clear() { v = 0; }
    constexpr void next() { v = v < Draw ? v + 1 : Draw; }
    constexpr bool isDraw() const { return v == Draw; }

    friend constexpr bool operator < (Rule50 rule50, Ply ply) { return rule50.v < ply; }

    friend ostream& operator << (ostream& out, Rule50 rule50) { return out << rule50.v; }

    friend istream& operator >> (istream& in, Rule50& rule50) {
        in >> rule50.v;
        if (in) { assert (0 <= rule50.v && rule50.v <= 100); }
        return in;
    }
};

class Position {
    Side::arrayOf<PositionSide> positionSide_; //copied from the parent, updated incrementally
    Side::arrayOf<Bb> occupied_; // both color pieces combined, updated from positionSide[] after each move

    Zobrist zobrist_; // incrementally updated position hash
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

    bool isSpecialMove(Square, Square) const;

protected:
    constexpr PositionSide& positionSide(Side side) { return positionSide_[side]; }
    constexpr const PositionSide& positionSide(Side side) const { return positionSide_[side]; }

    // all occupied squares by both sides from the given side point of view
    constexpr const Bb& occupied(Side side) const { return occupied_[side]; }

    // 'my' attackers count greater than 'op' attackers
    constexpr bool safeForMe(Square sq) const { return MY.attackersTo(sq).popcount() > OP.attackersTo(~sq).popcount(); }

    // 'op' attackers count greater than 'my' attackers
    constexpr bool safeForOp(Square sq) const { return OP.attackersTo(~sq).popcount() > MY.attackersTo(sq).popcount(); }

    // update the zobrist hash of incoming move
    void makeZobrist(const Position* parent, Square from, Square to) { zobrist_ = parent->createZobrist(from, to); }

    // update the position without updating the zobrist hash
    void makeMoveNoZobrist(const Position*, Square, Square);

    // update the position and its zobrist hash
    void makeMove(const Position*, Square, Square);

    void makeNullMove(const Position*);

    template <Side::_t> void setLegalEnPassant(Pi, Square);
    void setZobrist() { zobrist_ = generateZobrist(); }

    // number of halfmoves since last capture or pawn move
    constexpr const Rule50& rule50() const { return rule50_; }
    void setRule50(const Rule50& rule50) { rule50_ = rule50; }

    // convert internal move to be printable in UCI format
    UciMove uciMove(Square from, Square to) const { return UciMove{from, to, isSpecialMove(from, to)}; }

public:
    // position hash
    constexpr const Zobrist& zobrist() const { return zobrist_; }

    // position static evaluation
    Score evaluate() const;

    // make move directly inside position itself
    void makeMove(Square, Square);

// initial position setup in class UciPosition

    bool dropValid(Side, PieceType, Square);
    bool afterDrop();
};

#endif
