#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include <algorithm>
#include "io.hpp"
#include "typedefs.hpp"

// position evaluation score, fits in 14 bits
enum score_t {
    NoScore = -8192, //TRICK: assume two's complement
    MinusInfinity = NoScore + 1, // negative bound, no position should eval to it
    MinusMate = MinusInfinity + 1, // mated in 0, only even negative values for mated positions

    // negative mate range of scores (loss)

    MinEval = MinusMate + static_cast<i16_t>(MaxPly), // minimal (negative) non mate score bound for a position

    // negative evaluation range of scores

    DrawScore = 0, // only for 50 moves rule draw score

    // positive evalutation range of scores

    MaxEval = -MinEval, // maximal (positive) non mate score bound for a position

    // positive mate range of scores (win)

    PlusMate = -MinusMate, // mate in 0 (impossible), only odd positive values for mate positions
    PlusInfinity = -MinusInfinity, // positive bound, no position should eval to it
};

// position evaluation score, fits in 14 bits
struct Score {
    typedef score_t _t;
    _t v;

    constexpr bool isOk() const { return MinusInfinity <= v && v <= PlusInfinity; }
    constexpr bool isMate() const { assertOk(); return v < MinEval || MaxEval < v; }
    constexpr void assertOk() const { assert (isOk()); }
    constexpr void assertMate() const { assert (isMate()); }

    constexpr Score () : v{NoScore} {} // not isOk()
    constexpr Score (_t e) : v{e} {}
    constexpr explicit Score (int e) : v{static_cast<_t>(e)} { assertOk(); }
    constexpr operator const _t& () const { return v; }

    constexpr Score operator - () const { assertOk(); return Score{-v}; }
    constexpr Score operator ~ () const { assertOk(); return Score{-v}; }
    constexpr Score& operator += (int e) { assertOk(); v = static_cast<_t>(v + e); assertOk(); return *this; }
    constexpr Score& operator -= (int e) { assertOk(); v = static_cast<_t>(v - e); assertOk(); return *this; }

    constexpr friend Score operator + (Score s, int e) { return Score{static_cast<int>(s) + e}; }
    constexpr friend Score operator - (Score s, int e) { return Score{static_cast<int>(s) - e}; }
    constexpr friend Score operator + (Score s, Ply ply) { assert(s.isMate()); Score r{static_cast<int>(s) + ply}; assert(r.isMate()); return r; }
    constexpr friend Score operator - (Score s, Ply ply) { assert(s.isMate()); Score r{static_cast<int>(s) - ply}; assert(r.isMate()); return r; }

    // MinusMate + ply
    static constexpr Score checkmated(Ply ply) { return Score{MinusMate} + ply; }

    // clamp [MinEval, MaxEval] static evaluation to distinguish from mate scores
    constexpr Score clamp() const {
        if (v < MinEval) { return MinEval; }
        if (MaxEval < v) { return MaxEval; }
        assert (!isMate());
        return *this;
    }

    friend ostream& operator << (ostream& out, const Score& score) {
        out << " score ";

        if (score == NoScore) {
            return out << "none";
        }

        score.assertOk();

        if (score <= MinEval) {
            return out << "mate " << (MinusMate - score) / 2;
        }

        if (score >= MaxEval) {
            return out << "mate " << (PlusMate - score + 1) / 2;
        }

        assert (!score.isMate());
        return out << "cp " << static_cast<signed>(score.v);
    }
};

//https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
class PieceSquareTable {
public:
    static constexpr int PieceMatMax = 32; // initial chess position sum of non pawn pieces material points

    union element_type {
        struct PACKED {
            unsigned openingPst:14;
            unsigned endgamePst:14;

            unsigned queens:4;  // number of queens
            unsigned rooks:4;   // number of rooks
            unsigned bishops:4; // number of bishops
            unsigned knights:4; // number of knights
            unsigned pawns:4;   // number of pawns

            unsigned piecesMat:8; // sum of non pawn pieces material points (pawn = 1)
            unsigned totalMat:8;  // sum of all pieces material points (pawn = 1)
        } s;
        u64_t v;

        constexpr const element_type& operator += (const element_type& o) { v += o.v; return *this; }
        constexpr const element_type& operator -= (const element_type& o) { v -= o.v; return *this; }

        constexpr Score score(int material) const {
            auto stage = std::min(material, PieceMatMax);
            return Score{(s.openingPst*stage + s.endgamePst*(PieceMatMax - stage)) / PieceMatMax};
        }
    };

protected:
    PieceType::arrayOf< Square::arrayOf<element_type> > pst;

public:
    PieceSquareTable ();
    constexpr const element_type& operator() (PieceType ty, Square sq) const { return pst[ty][sq]; }
};

extern const PieceSquareTable pieceSquareTable;

class Evaluation {
public:
    typedef PieceSquareTable::element_type _t;

private:
    _t v;

    constexpr void from(PieceType::_t ty, Square sq) { v -= pieceSquareTable(ty, sq); }
    constexpr void to(PieceType::_t ty, Square sq) { v += pieceSquareTable(ty, sq); }

public:
    constexpr Evaluation () : v{} {}

    static Score evaluate(const Evaluation& my, const Evaluation& op) {
        return my.v.score(my.v.s.piecesMat) - op.v.score(op.v.s.piecesMat);
    }

    constexpr Score score(PieceType ty, Square sq) const {
        return pieceSquareTable(ty, sq).score(v.s.piecesMat);
    }

    void drop(PieceType ty, Square t) { to(ty, t); }
    void capture(PieceType ty, Square f) { assert (ty != King); from(ty, f); }
    void move(PieceType ty, Square f, Square t) { assert (f != t); from(ty, f); to(ty, t); }

    void promote(Square f, Square t, PromoType ty) {
        assert (f.on(Rank7) && t.on(Rank8));
        from(Pawn, f);
        to(ty, t);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        from(King, kingFrom); from(Rook, rookFrom);
        to(Rook, rookTo); to(King, kingTo);
    }

    constexpr int count(PieceType ty) const {
        switch (ty) {
            case Queen:
                return v.s.queens;
            case Rook:
                return v.s.rooks;
            case Bishop:
                return v.s.bishops;
            case Knight:
                return v.s.knights;
            case Pawn:
                return v.s.pawns;
            default:
                assert (false);
                return 0;
        }
    }

    // 10, 5, 3, 3, 0
    constexpr int piecesMat() const {
        return v.s.piecesMat;
    }

    // 12, 6, 4, 4, 1
    constexpr int material() const {
        return v.s.totalMat;
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return (v.s.queens | v.s.rooks | v.s.pawns) != 0;
    }

};

#endif
