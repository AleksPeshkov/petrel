#ifndef NNUE_HPP
#define NNUE_HPP

#ifdef __AVX2__
    #define HAS_AVX2 1
    #include <immintrin.h>
#else
    #define HAS_AVX2 0
#endif

#include <bit>
#include "Index.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));
using vi32x8_t  = i32_t __attribute__((vector_size(32)));

static constexpr int vector_size = sizeof(vi16x16_t);

constexpr vi16x16_t all16x16(i16_t i) { return vi16x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }
constexpr vi32x8_t all32x8(i32_t i) { return vi32x8_t{ i,i,i,i, i,i,i,i}; }

#if HAS_AVX2

inline vi16x16_t max(vi16x16_t a, vi16x16_t b) {
    return _mm256_max_epi16(a, b);
}

inline vi16x16_t min(vi16x16_t a, vi16x16_t b) {
    return _mm256_min_epi16(a, b);
}

// Portable equivalent of _mm256_madd_epi16
inline vi32x8_t madd(vi16x16_t a, vi16x16_t b) {
    return _mm256_madd_epi16(a, b);
}

#else

template <typename vector>
constexpr vector max(vector a, vector b) {
    return ((a >= b) & a) | ((a < b) & b);
}

template <typename vector>
constexpr vector min(vector a, vector b) {
    return ((a >= b) & b) | ((a < b) & a);
}

// Portable equivalent of _mm256_madd_epi16
inline vi32x8_t madd(vi16x16_t a, vi16x16_t b) {
    using vi32x16_t = i32_t __attribute__((vector_size(64)));

    auto a32 = __builtin_convertvector(a, vi32x16_t);
    auto b32 = __builtin_convertvector(b, vi32x16_t);
    auto product = a32 * b32;

    vi32x8_t even = __builtin_shufflevector(product, product, 0, 2, 4, 6, 8, 10, 12, 14);
    vi32x8_t odd  = __builtin_shufflevector(product, product, 1, 3, 5, 7, 9, 11, 13, 15);

    return even + odd;
}

#endif

// NNUE evaluation. Net architecture and constants make it
// compatible with the simple example provided by the bullet trainer:
// https://github.com/jw1912/bullet/blob/main/examples/simple.rs

// The architecture is simple perspective (768 -> HIDDEN_SIZE)x2 -> 1
struct alignas(64) Nnue {
    static constexpr int HIDDEN_SIZE = 128;
    static constexpr i32_t SCALE = 400;
    static constexpr i16_t QA = 255;
    static constexpr i32_t QB = 64;

    using _t = vi16x16_t;

    using IndexL0 = Index<768>;
    using IndexL1 = Index<HIDDEN_SIZE / (vector_size/sizeof(i16_t))>;
    using IndexL2 = Index<2*IndexL1::Size>;

    using ArrayL1 = IndexL1::arrayOf<_t>;
    using ArrayL2 = IndexL2::arrayOf<_t>;

    IndexL0::arrayOf<ArrayL1> inputWeights;
    ArrayL1 hiddenBiases;
    ArrayL2 outputWeights;
    i16_t outputBias;

    // SCReLU activation function
    constexpr i32_t evaluate(const ArrayL2& accumulator) {
        auto sum = all32x8(0);

        for (auto i : IndexL2::range()) {
            auto v16 = max(accumulator[i], all16x16(0)); // ReLU
            v16 = min(v16, all16x16(QA)); // CReLU
            sum += madd(v16, v16 * outputWeights[i]); //SCReLU
        }

        i32_t output = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
        return (output / QA + outputBias) * SCALE / (QA * QB);
    }

    // load from embedded binary data, defined in main.cpp
    void setEmbeddedEval();

};

extern Nnue nnue;

class alignas(vector_size) AccumulatorSide {
    constexpr static auto& w = nnue.inputWeights;
    constexpr static auto& b = nnue.hiddenBiases;

    //TODO: reshape input weights during net loading
    constexpr static Nnue::IndexL0 indexL0(Side::_t si, PieceType::_t ty, Square sq) {
        constexpr int pieceType[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
        return Nnue::IndexL0{ 6*64*(si) + 64*pieceType[ty] + (~sq) };
    }

    using Index = Nnue::IndexL1;
    Index::arrayOf<Nnue::_t> v;

public:
    constexpr void clear() {
        for (auto i : Index::range()) {
            v[i] = b[i];
        }
    }

    void drop(Side::_t si, PieceType ty, Square to) {
        for (auto i : Index::range()) {
            v[i] += w[indexL0(si, ty, to)][i];
        }
    }

    void move(Side::_t si, PieceType::_t ty, Square from, Square to) {
        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, ty, from)][i];
            v[i] += w[indexL0(si, ty, to)][i];
        }
    }

    void move(Side::_t si, PieceType::_t ty, Square from, Square to, NonKingType::_t captured) {
        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, ty, from)][i];
            v[i] += w[indexL0(si, ty, to)][i];
            v[i] -= w[indexL0(~si, captured, to)][i];
        }
    }

    void promote(Side::_t si, PromoType promoted, Square from, Square to) {
        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, promoted, to)][i];
        }
    }

    void promote(Side::_t si, PromoType promoted, Square from, Square to, NonKingType::_t captured) {
        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, promoted, to)][i];
            v[i] -= w[indexL0(~si, captured, to)][i];
        }
    }

    void ep(Side::_t si, Square from, Square to, Square capture) {
        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, Pawn, to)][i];
            v[i] -= w[indexL0(~si, Pawn, capture)][i];
        }
    }

    void castle(Side::_t si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        for (auto i : Index::range()) {
            v[i] -= w[indexL0(si, King, kingFrom)][i];
            v[i] -= w[indexL0(si, Rook, rookFrom)][i];
            v[i] += w[indexL0(si, Rook, rookTo)][i];
            v[i] += w[indexL0(si, King, kingTo)][i];
        }
    }

};

//TRICK: accumulator assumes updates from NOT side to move
class Accumulator {
    Side::arrayOf<AccumulatorSide> side;

public:
    auto evaluate() const {
        auto accumulator = std::bit_cast<Nnue::IndexL2::arrayOf<vi16x16_t>>(side);
        return nnue.evaluate(accumulator);
    }

    void clear() {
        side[My].clear();
        side[Op].clear();
    }

    void copyParent(const Accumulator& parent) {
        side[My] = parent.side[Op];
        side[Op] = parent.side[My];
    }

    void swap() {
        std::swap(side[My], side[Op]);
    }

    void drop(Side::_t si, PieceType ty, Square to) {
        side[si].drop(My, ty, to);
        side[~si].drop(Op, ty, ~to);
    }

    void move(PieceType::_t ty, Square from, Square to) {
        side[Op].move(My, ty, from, to);
        side[My].move(Op, ty, ~from, ~to);
    }

    void move(PieceType::_t ty, Square from, Square to, NonKingType::_t captured) {
        side[Op].move(My, ty, from, to, captured);
        side[My].move(Op, ty, ~from, ~to, captured);
    }

    void promote(PromoType promoted, Square from, Square to) {
        side[Op].promote(My, promoted, from, to);
        side[My].promote(Op, promoted, ~from, ~to);
    }

    void promote(PromoType promoted, Square from, Square to, NonKingType::_t captured) {
        side[Op].promote(My, promoted, from, to, captured);
        side[My].promote(Op, promoted, ~from, ~to, captured);
    }

    void ep(Square from, Square to, Square capture) {
        side[Op].ep(My, from, to, capture);
        side[My].ep(Op, ~from, ~to, ~capture);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        side[Op].castle(My, kingFrom, kingTo, rookFrom, rookTo);
        side[My].castle(Op, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
