#ifndef NNUE_HPP
#define NNUE_HPP

#include <immintrin.h>
#include "typedefs.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));
using vi32x8_t  = i32_t __attribute__((vector_size(32)));

constexpr vi16x16_t all16x16(i16_t i) { return vi16x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }
constexpr vi32x8_t all32x8(i32_t i) { return vi32x8_t{ i,i,i,i, i,i,i,i}; }

constexpr vi16x16_t max(vi16x16_t a, vi16x16_t b) {
    return ((a >= b) & a) | ((a < b) & b);
}

constexpr vi16x16_t min(vi16x16_t a, vi16x16_t b) {
    return ((a >= b) & b) | ((a < b) & a);
}

// Made compatible with net files made for
// Publius chess engine by Pawel Koziol

// NNUE evaluation. Net architecture and constants make it
// equivalent to the simple example provided by the bullet trainer:
// https://github.com/jw1912/bullet/blob/main/examples/simple.rs
// The architecture is (768 -> HIDDEN_SIZE)x2 -> 1

struct alignas(64) Nnue {
    static constexpr int HIDDEN_SIZE = 128;
    static constexpr i32_t SCALE = 400;
    static constexpr i16_t QA = 255;
    static constexpr i32_t QB = 64;

    using IndexL0 = Index<768>;
    using IndexL1 = Index<HIDDEN_SIZE/16>;
    using IndexL2 = Index<2*IndexL1::Size>;

    IndexL0::arrayOf<IndexL1::arrayOf<vi16x16_t>> inputWeights;
    IndexL1::arrayOf<vi16x16_t> hiddenBiases;
    IndexL2::arrayOf<vi16x16_t> outputWeights;
    i16_t outputBias;

    // SCReLU activation function
    constexpr i32_t evaluate(const IndexL2::arrayOf<vi16x16_t>& hidden) {
        auto output = all32x8(0);
        for (auto i : IndexL2::range()) {
            // ReLU
            vi16x16_t v0 = max(hidden[i], all16x16(0));

            // CReLU
            v0 = min(v0, all16x16(QA));

            //SCReLU
            vi16x16_t v1 = v0 * outputWeights[i];
            vi32x8_t v2 = _mm256_madd_epi16(v0, v1);

            output += v2;
        }

        i32_t scalar =
            output[0] + output[1] + output[2] + output[3] +
            output[4] + output[5] + output[6] + output[7];

        return (scalar / QA + outputBias) * SCALE / (QA * QB);
    }

};

extern Nnue nnue;

class alignas(64) Accumulator {
    //TODO: reshape input weights during net loading
    constexpr static Nnue::IndexL0 indexL0(Side::_t si, PieceType::_t ty, Square sq) {
        constexpr int pieceType[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
        return Nnue::IndexL0{ 6*64*(si) + 64*pieceType[ty] + (~sq) };
    }

    using _t = Nnue::IndexL1::arrayOf<vi16x16_t>;
    _t v;

    void move(Side::_t si, PieceType::_t ty, Square from, Square to) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, ty, from)][i];
            v[i] += w[indexL0(si, ty, to)][i];
        }
    }

    void move(Side::_t si, PieceType::_t ty, Square from, Square to, NonKingType::_t captured) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, ty, from)][i];
            v[i] += w[indexL0(si, ty, to)][i];
            v[i] -= w[indexL0(~si, captured, to)][i];
        }
    }

    void promote(Side::_t si, PromoType promoted, Square from, Square to) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, promoted, to)][i];
        }
    }

    void promote(Side::_t si, PromoType promoted, Square from, Square to, NonKingType::_t captured) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, promoted, to)][i];
            v[i] -= w[indexL0(~si, captured, to)][i];
        }
    }

    void ep(Side::_t si, Square from, Square to, Square capture) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, Pawn, from)][i];
            v[i] += w[indexL0(si, Pawn, to)][i];
            v[i] -= w[indexL0(~si, Pawn, capture)][i];
        }
    }

    void castle(Side::_t si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        for (auto i : Nnue::IndexL1::range()) {
            v[i] -= w[indexL0(si, King, kingFrom)][i];
            v[i] -= w[indexL0(si, Rook, rookFrom)][i];
            v[i] += w[indexL0(si, Rook, rookTo)][i];
            v[i] += w[indexL0(si, King, kingTo)][i];
        }
    }

public:
    constexpr static auto& w = nnue.inputWeights;
    constexpr static auto& b = nnue.hiddenBiases;

    constexpr void clear() {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] = b[i];
        }
    }

    const auto& operator[](Nnue::IndexL1 i) const { return v[i]; }

    void drop(Side::_t si, PieceType ty, Square to) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] += w[indexL0(si, ty, to)][i];
        }
    }

    static void move(Side::arrayOf<Accumulator>& a, PieceType::_t ty, Square from, Square to) {
        a[Op].move(My, ty, from, to);
        a[My].move(Op, ty, ~from, ~to);
    }

    static void move(Side::arrayOf<Accumulator>& a, PieceType::_t ty, Square from, Square to, NonKingType::_t captured) {
        a[Op].move(My, ty, from, to, captured);
        a[My].move(Op, ty, ~from, ~to, captured);
    }

    static void promote(Side::arrayOf<Accumulator>& a, PromoType promoted, Square from, Square to) {
        a[Op].promote(My, promoted, from, to);
        a[My].promote(Op, promoted, ~from, ~to);
    }

    static void promote(Side::arrayOf<Accumulator>& a, PromoType promoted, Square from, Square to, NonKingType::_t captured) {
        a[Op].promote(My, promoted, from, to, captured);
        a[My].promote(Op, promoted, ~from, ~to, captured);
    }

    static void ep(Side::arrayOf<Accumulator>& a, Square from, Square to, Square capture) {
        a[Op].ep(My, from, to, capture);
        a[My].ep(Op, ~from, ~to, ~capture);
    }

    static void castle(Side::arrayOf<Accumulator>& a, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        a[Op].castle(My, kingFrom, kingTo, rookFrom, rookTo);
        a[My].castle(Op, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
