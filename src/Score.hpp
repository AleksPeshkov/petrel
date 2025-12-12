#ifndef SCORE_HPP
#define SCORE_HPP

#include "Index.hpp"

// position evaluation score, fits in 14 bits
enum score_enum : i16_t {
    NoScore = -8192, //TRICK: assume two's complement
    MinusInfinity = NoScore + 1, // negative bound, no position should eval to it
    MinusMate = MinusInfinity + 1, // mated in 0, only even negative values for mated positions

    // negative mate range of scores (loss)

    MinEval = MinusMate + static_cast<i16_t>(MaxPly+1), // minimal (negative) non mate score bound for a position

    // negative evaluation range of scores

    DrawScore = 0,

    // positive evalutation range of scores

    MaxEval = -MinEval, // maximal (positive) non mate score bound for a position

    // positive mate range of scores (win)

    PlusMate = -MinusMate, // mate in 0 (impossible), only odd positive values for mate positions
    PlusInfinity = -MinusInfinity, // positive bound, no position should eval to it
};

// position evaluation score, fits in 14 bits
struct Score {
    static constexpr int Mask = 0x3fff;
    using _t = score_enum;
    _t v;

    constexpr bool isOk() const { return MinusInfinity <= v && v <= PlusInfinity; }
    constexpr bool isEval() const { assertOk(); return MinEval <= v && v <= MaxEval; }
    constexpr bool isMate() const { assertOk(); return !isEval(); }
    constexpr void assertOk() const { assert (isOk()); }
    constexpr void assertEval() const { assert (isEval()); }
    constexpr void assertMate() const { assert (isMate()); }

    constexpr Score () : v{NoScore} {} // not isOk()
    constexpr Score (_t e) : v{e} {}

    template <typename N>
    constexpr static Score clamp(N e) {
        return Score{static_cast<Score::_t>(
            std::clamp<N>(e, MinEval, MaxEval)
        )};
    }

    constexpr explicit Score (int e) : v{static_cast<_t>(e)} { assertOk(); }
    constexpr operator const _t& () const { return v; }

    constexpr Score operator - () const { assertOk(); return Score{-v}; }
    constexpr Score operator ~ () const { assertOk(); return Score{-v}; }

    constexpr friend Score operator + (Score s, int e) { s.assertOk(); Score r{static_cast<int>(s) + e}; r.assertOk(); return r; }
    constexpr friend Score operator - (Score s, int e) { s.assertOk(); Score r{static_cast<int>(s) - e}; r.assertOk(); return r; }
    constexpr friend Score operator + (Score s, Ply p) { s.assertMate(); Score r{static_cast<int>(s) + p}; r.assertMate(); return r; }
    constexpr friend Score operator - (Score s, Ply p) { s.assertMate(); Score r{static_cast<int>(s) - p}; r.assertMate(); return r; }

    // MinusMate + ply
    static constexpr Score checkmated(Ply ply) { return Score{MinusMate} + ply; }

    // clamp [MinEval, MaxEval] static evaluation to distinguish from mate scores
    constexpr Score clamp() const {
        if (v < MinEval) { return MinEval; }
        if (MaxEval < v) { return MaxEval; }
        assert (isEval());
        return *this;
    }

    constexpr Score toTt(Ply ply) const {
        assertOk();
        if (v < MinEval) { assertMate(); Score r = *this - ply; r.assertMate(); return r; }
        if (MaxEval < v) { assertMate(); Score r = *this + ply; r.assertMate(); return r; }
        return *this;
    }

    constexpr Score fromTt(Ply ply) const {
        assertOk();
        if (*this < MinEval - ply) { assertMate(); Score r = *this + ply; r.assertMate(); return r; }
        if (MaxEval + ply < *this) { assertMate(); Score r = *this - ply; r.assertMate(); return r; }
        return *this;
    }

    friend ostream& operator << (ostream& out, const Score& score) {
        out << " score ";

        if (score == NoScore) {
            return out << "none";
        }

        score.assertOk();

        if (score < MinEval) {
            return out << "mate " << (MinusMate - score) / 2;
        }

        if (MaxEval < score) {
            return out << "mate " << (PlusMate - score + 1) / 2;
        }

        score.assertEval();
        return out << "cp " << static_cast<signed>(score.v);
    }
};

class PieceCountTable {
    union element_type {
        struct {
            u16_t centipawns; // material evaluation (pawn = 80)
            u8_t phase; // game phase: sum of nonpawn nonking pieces in pawn units (startpos total is 32)
            NonKingType::arrayOf<u8_t> count; // number of pieces of the given type
        } s;
        u64_t n;
        static_assert (sizeof(s) == sizeof(n));
    };

    PieceType::arrayOf<element_type> v;

public:
    using _t = element_type;

    constexpr PieceCountTable () {
        constexpr u16_t centipawns[] = { 960, 480, 320, 320, 80, 0 }; // material eval: 12/6/4/4/1 * 80cp
        constexpr u8_t phase[] = { 10, 5, 3, 3, 0, 0 }; // pawn units for game phase, startpos sum is 32

        for (auto ty : PieceType::range()) {
            v[ty].s.centipawns = centipawns[ty];
            v[ty].s.phase = phase[ty];

            for (auto i : NonKingType::range()) {
                v[ty].s.count[i] = (ty == i);
            }
        }
    }

    constexpr const _t& operator[] (PieceType::_t ty) const { return v[ty]; }
};

extern const PieceCountTable pieceCountTable;

class Material {
    using _t = PieceCountTable::_t;
    _t v;

public:
    constexpr Material () { v.n = 0; }

    void drop(PieceType ty) { v.n += ::pieceCountTable[ty].n; }
    void clear(NonKingType ty) { v.n -= ::pieceCountTable[ty].n; }

    void promote(PromoType ty) {
        clear(NonKingType{Pawn});
        drop(PieceType{ty});
    }

    // material eval in centipawns
    constexpr Score centipawns() const {
        return Score::clamp(v.s.centipawns);
    }

    // 10, 5, 3, 3, 0 (startpos = 32)
    constexpr int phase() const {
        return v.s.phase;
    }

    constexpr int count(NonKingType::_t ty) const {
        return v.s.count[ty];
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return count(Queen) + count(Rook) + count(Pawn) > 0;
    }

};

#endif
