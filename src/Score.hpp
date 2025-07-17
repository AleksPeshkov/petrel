#ifndef SCORE_HPP
#define SCORE_HPP

#include <algorithm>
#include "typedefs.hpp"
#include "out.hpp"

// position evaluation score, fits in 14 bits
enum score_t : i16_t {
    NoScore = -8192, //TRICK: assume two's complement
    MinusInfinity = NoScore + 1, // negative bound, no position should eval to it
    MinusMate = MinusInfinity +1, // mated in 0, only even negative values for mated positions
    // negative mate range of scores
    MinEval = MinusMate + static_cast<i16_t>(MaxPly), // minimal (negative) non mate score bound for a position
    MinusDraw = -1, // negative draw score when opposite side forced draw
    // negative evaluation range of scores
    DrawScore = 0, // only for 50 moves rule draw score
    PlusDraw = 1, // positive draw score when this side forced draw
    // positive evalutation range of scores
    MaxEval = -MinEval, // maximal (positive) non mate score bound for a position
    // negative mate range of scores
    PlusMate = -MinusMate, // mate in 0 (impossible), only odd positive values for mate positions
    PlusInfinity = -MinusInfinity, // positive bound, no position should eval to it
};

// position evaluation score, fits in 14 bits
struct Score {
    typedef score_t _t;
    _t v;

    constexpr Score (int e) : v{static_cast<_t>(e)} {}
    constexpr operator const _t& () const { return v; }

    constexpr Score operator - () { return Score{-v}; }
    constexpr const Score& operator += (int e) { v = static_cast<_t>(v + e); return *this; }
    constexpr const Score& operator -= (int e) { v = static_cast<_t>(v - e); return *this; }
    constexpr Score operator + (int e) { return Score{v + e}; }
    constexpr Score operator - (int e) { return Score{v - e}; }

    constexpr bool isMate() const { return v < MinEval || MaxEval < v; }
    static constexpr Score checkmated(Ply ply) { return MinusMate + static_cast<int>(ply); }

    // clamp evaluation from special scores
    constexpr Score clamp() const {
        if (v < MinEval) { return MinEval; }
        if (v == MinusDraw) { return MinusDraw - 1; }
        if (v == PlusDraw)  { return PlusDraw + 1; }
        if (MaxEval < v) { return MaxEval; }
        return v;
    }

    friend out::ostream& operator << (out::ostream& out, Score score) {
        if (score == NoScore) {
            return out;
        }

        if (score <= MinEval) {
            return out << " score mate " << (MinusMate - score) / 2;
        }

        if (score >= MaxEval) {
            return out << " score mate " << (PlusMate - score + 1) / 2;
        }

        return out << " score cp " << static_cast<signed>(score.v);
    }
};


#endif
