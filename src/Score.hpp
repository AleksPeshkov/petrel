#ifndef SCORE_HPP
#define SCORE_HPP

#include "Index.hpp"

static constexpr int ScoreBitSize = 14;

// position evaluation score, fits in 14 bits
enum score_enum : i16_t {
    NoScore = -singleton(ScoreBitSize-1), //TRICK: assume two's complement
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
class Score {
    using _t = score_enum;
    _t v;
public:
    static constexpr int Mask = singleton(ScoreBitSize) - 1;

    explicit constexpr Score (_t e) : v{e} {}
    friend constexpr Score operator""_cp(unsigned long long);
    constexpr static Score clampEval(int e) { return Score{static_cast<Score::_t>( std::clamp<int>(e, MinEval, MaxEval) )}; }

    constexpr Score minus1() const { return Score{static_cast<_t>(v-1)}; }
    constexpr Score operator - () const { assertOk(); return Score{static_cast<_t>(-v)}; }
    constexpr friend Score operator + (Score s, Score d) { s.assertEval(); return Score::clampEval(s.v + d.v); }
    constexpr friend Score operator - (Score s, Score d) { s.assertEval(); return Score::clampEval(s.v - d.v); }
    constexpr friend Score operator + (Score s, Ply p) { s.assertMate(); Score r{static_cast<_t>(s.v + p)}; r.assertMate(); return r; }
    constexpr friend Score operator - (Score s, Ply p) { s.assertMate(); Score r{static_cast<_t>(s.v - p)}; r.assertMate(); return r; }

    constexpr void assertOk() const { assert (MinusInfinity <= v && v <= PlusInfinity); }
    constexpr bool isEval() const { assertOk(); return MinEval <= v && v <= MaxEval; }
    constexpr void assertEval() const { assert (isEval()); }
    constexpr void assertMate() const { assert (!isEval()); }

    friend constexpr auto operator <=> (Score, Score) = default;

    // MinusMate + ply
    static constexpr Score checkmated(Ply ply) { return Score{MinusMate} + ply; }

    constexpr unsigned toTt(Ply ply) const {
        Score score{v};

        if (score.v == NoScore) {
            return 0;
        }

        if (score.v < MinEval) {
            score = score - ply;
        }
        else if (MaxEval < score.v) {
            score = score + ply;
        }
        else {
            score.assertEval();
        }

        // convert signed to unsigned
        return static_cast<unsigned>(score.v - NoScore);
    }

    static constexpr Score fromTt(unsigned n, Ply ply) {
        // convert unsigned to signed
        Score score{static_cast<_t>(static_cast<int>(n) + NoScore)};

        if (score.v < MinEval) {
            score.assertMate(); score = score + ply; score.assertMate();
        }
        else if (MaxEval < score.v) {
            score.assertMate(); score = score - ply; score.assertMate();
        }
        else {
            score.assertEval();
        }

        return score;
    }

    friend ostream& operator << (ostream& out, Score score) {
        out << " score ";

        if (score == Score{NoScore}) {
            return out << "none";
        }

        score.assertOk();

        if (score.v < MinEval) {
            return out << "mate " << (MinusMate - score.v) / 2;
        }

        if (MaxEval < score.v) {
            return out << "mate " << (PlusMate - score.v + 1) / 2;
        }

        score.assertEval();
        return out << "cp " << static_cast<signed>(score.v);
    }
};

constexpr Score operator""_cp(unsigned long long n) { return Score{static_cast<Score::_t>(n)}; }

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
        constexpr u8_t phase[] = { 12, 6, 4, 4, 0, 0 }; // pawn units for game phase, startpos sum is 40

        for (auto ty : range<PieceType>()) {
            v[ty].s.centipawns = centipawns[ty];
            v[ty].s.phase = phase[ty];

            for (auto i : range<NonKingType>()) {
                v[ty].s.count[i] = (ty == i);
            }
        }
    }

    constexpr const _t& operator[] (PieceType ty) const { return v[ty]; }
};

extern const PieceCountTable pieceCountTable;

class Material {
    using _t = PieceCountTable::_t;
    _t v;

public:
    constexpr Material () { v.n = 0; }

    void drop(PieceType ty) { v.n += ::pieceCountTable[ty].n; }
    void clear(NonKingType ty) { v.n -= ::pieceCountTable[PieceType{ty}].n; }

    void promote(PromoType ty) {
        clear(NonKingType{Pawn});
        drop(PieceType{ty});
    }

    // 12, 6, 4, 4, 0 (startpos = 40)
    constexpr int phase() const {
        return v.s.phase;
    }

    constexpr int count(NonKingType::_t ty) const {
        return v.s.count[NonKingType{ty}];
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return count(Pawn) || count(Rook) || count(Queen);
    }

};

#endif
