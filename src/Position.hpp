#ifndef POSITION_HPP
#define POSITION_HPP

#include "nnue.hpp"
#include "PositionSide.hpp"
#include "Score.hpp"

// side to move
#define MY positionSide(Side{My})

// opponent side
#define OP positionSide(Side{Op})

// all occupied squares by both sides from the current side point of view
#define OCCUPIED occupied(Side{My})

#define OP_OCCUPIED occupied(Side{Op})

// number of halfmoves without capture or pawn move
class Rule50 {
    int v_;
    static constexpr int Draw = 100;

public:
    constexpr Rule50() : v_{0} {}
    constexpr void clear() { v_ = 0; }
    constexpr void next() { v_ = v_ < Draw ? v_ + 1 : Draw; }
    constexpr bool isDraw() const { return v_ == Draw; }

    friend constexpr bool operator == (Rule50 rule50, Ply ply) { return rule50.v_ == ply.v(); }
    friend constexpr bool operator <  (Rule50 rule50, Ply ply) { return rule50.v_ < ply.v(); }

    friend ostream& operator << (ostream& os, Rule50 rule50) { return os << rule50.v_; }

    friend istream& operator >> (istream& is, Rule50& rule50) {
        is >> rule50.v_;
        if (is) { assert (0 <= rule50.v_ && rule50.v_ <= 100); }
        return is;
    }
};

class Zobrist {
    Z v_;

    constexpr void my(Z::Index ty, Square sq) { v_ = v_ ^ Z{ty, sq}; }
    constexpr void op(Z::Index ty, Square sq) { v_ = v_ ^ ~Z{ty, sq}; }

public:
    constexpr Zobrist () : v_{} {}
    constexpr explicit Zobrist (Z z) : v_{z} {}
    constexpr Zobrist (Zobrist my, Zobrist op) : v_{my.v_ ^ ~op.v_} {}

    constexpr Z v() const { return v_; }

    constexpr Zobrist& flip() { v_ = ~v_; return *this; }

    void operator () (PieceType ty, Square sq) { my(ty, sq); }
    void castling(Square sq)  { assert (sq.on(Rank1)); my(Z::Castling, sq); }
    void enPassant(Square sq) { assert (sq.on(Rank4)); my(Z::EnPassant, sq); }

    void opCapture(NonKingType ty, Square sq) { op(ty, sq); }
    void opCastling(Square sq)  { assert (sq.on(Rank1)); op(Z::Castling, sq); }
    void opEnPassant(Square sq) { assert (sq.on(Rank4)); op(Z::EnPassant, sq); }

    void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        my(ty, from);
        my(ty, to);
    }

    void promote(Square from, PromoType ty, Square to) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        my(Pawn, from);
        my(ty, to);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom.on(Rank1));
        assert (kingTo.on(Rank1));
        assert (rookFrom.on(Rank1));
        assert (rookTo.on(Rank1));
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);
        my(King, kingFrom);
        my(King, kingTo);
        my(Rook, rookFrom);
        my(Rook, rookTo);
    }
};

class Position {
    Accumulator accumulator; // NNUE evaluation accumulators (separate for each side)
    Side::arrayOf<PositionSide> positionSide_; //copied from the parent, updated incrementally
    Side::arrayOf<Bb> occupied_; // both color pieces combined, updated from positionSide[] after each move

    Zobrist zobrist_; // incrementally updated position hash
    Rule50 rule50_; // number of halfmoves since last capture or pawn move, incremented or reset by makeMove()

    template <Side::_t> void updateSliderAttacks(PiMask);
    template <Side::_t> void updateSliderAttacks(PiMask, PiMask);

    enum MakeMoveFlags { WithZobrist = 0b01, WithEval = 0b10, ZobristAndEval = WithZobrist | WithEval };
    template <Side::_t, MakeMoveFlags> void makeMove(Square, Square);

    template <Side::_t> Zobrist generateZobrist() const;

    // update Zobrist key incrementally
    Zobrist createZobrist(Square, Square) const;

    // calculate Zobrist key from scratch
    Zobrist generateZobrist() const;

    bool isSpecialMove(Square, Square) const;
    void copyParent(const Position* parent);

protected:
    constexpr PositionSide& positionSide(Side side) { return positionSide_[side]; }

    // all occupied squares by both sides from the given side point of view
    constexpr Bb occupied(Side side) const { return occupied_[side]; }

    // myAttackers > opAttackers
    bool safeForMe(Square sq) const {
        int myAttackers = MY.countAttackersTo(sq, OCCUPIED);
        int opAttackers = OP.countAttackersTo(~sq, OP_OCCUPIED);
        return myAttackers > opAttackers;
    }

    // opAttackers > myAttackers
    bool safeForOp(Square sq) const {
        int myAttackers = MY.countAttackersTo(sq, OCCUPIED);
        int opAttackers = OP.countAttackersTo(~sq, OP_OCCUPIED);
        return opAttackers > myAttackers;
    }

    // update the zobrist hash of incoming move
    void makeZobrist(const Position* parent, Square from, Square to) { zobrist_ = parent->createZobrist(from, to); }

    // update the position without updating the zobrist hash (because it already updated)
    void makeMoveNoZobrist(const Position*, Square, Square);

    // make move directly inside position itself
    void makeMove(Square, Square);

    // update the position and its zobrist hash
    void makeMove(const Position*, Square, Square);

    void makeNullMove(const Position*);

    template <Side::_t> void setLegalEnPassant(Square);
    void setZobrist() { zobrist_ = generateZobrist(); }

    // number of halfmoves since last capture or pawn move
    constexpr Rule50 rule50() const { return rule50_; }
    void setRule50(Rule50 rule50) { rule50_ = rule50; }

    // convert internal move to be printable in UCI format
    UciMove uciMove(Square from, Square to) const { return UciMove{from, to, isSpecialMove(from, to)}; }

public:
    // position hash
    constexpr Z z() const { return zobrist_.v(); }

    // position static evaluation
    Score evaluate() const;

    // update the position inside itself and its zobrist hash, but not NNUE accumulators
    void makeMoveNoEval(Square, Square);

    // [0..6] startpos = 6, queens exchanged = 4, R vs R endgame = 1
    constexpr auto gamePhase() const { return Material::gamePhase(MY.material(), OP.material()); }

// initial position setup in class UciPosition

    void clear(); // init accumulators
    bool dropValid(Side, PieceType, Square);
    bool afterDrop();
    bool setEnPassant(File);

// needed public for unit tests
    constexpr const PositionSide& positionSide(Side side) const { return positionSide_[side]; }

    // my passed pawns
    Bb bbPassedPawns() const;
};

#endif
