#ifndef NNUE_HPP
#define NNUE_HPP

#include <immintrin.h>
#include "Index.hpp"

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

// NNUE evaluation. Net architecture and constants make it
// compatible with the simple example provided by the bullet trainer:
// https://github.com/jw1912/bullet/blob/main/examples/simple.rs

// The architecture is simple perspective (768 -> HIDDEN_SIZE)x2 -> 1
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
        auto sum = all32x8(0);

        for (auto i : IndexL2::range()) {
            auto v0 = max(hidden[i], all16x16(0)); // ReLU
            v0 = min(v0, all16x16(QA)); // CReLU
            vi32x8_t v2 = _mm256_madd_epi16(v0, v0 * outputWeights[i]); //SCReLU

            sum += v2;
        }

        i32_t output = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
        return (output / QA + outputBias) * SCALE / (QA * QB);
    }

};

extern Nnue nnue;

class alignas(32) AccumulatorSide {
    constexpr static auto& w = nnue.inputWeights;
    constexpr static auto& b = nnue.hiddenBiases;

    //TODO: reshape input weights during net loading
    constexpr static Nnue::IndexL0 indexL0(Side::_t si, PieceType::_t ty, Square sq) {
        constexpr int pieceType[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
        return Nnue::IndexL0{ 6*64*(si) + 64*pieceType[ty] + (~sq) };
    }

    using _t = Nnue::IndexL1::arrayOf<vi16x16_t>;
    _t v;

public:
    constexpr void clear() {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] = b[i];
        }
    }

    void drop(Side::_t si, PieceType ty, Square to) {
        for (auto i : Nnue::IndexL1::range()) {
            v[i] += w[indexL0(si, ty, to)][i];
        }
    }

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

};

//TRICK: accumulator assumes updates from NOT side to move
class Accumulator {
    Side::arrayOf<AccumulatorSide> a;

public:
    auto evaluate() const {
        auto acc2 = reinterpret_cast<const Nnue::IndexL2::arrayOf<vi16x16_t>&>(a);
        return nnue.evaluate(acc2);
    }

    void clear() {
        a[My].clear();
        a[Op].clear();
    }

    void copyParent(const Accumulator& parent) {
        a[My] = parent.a[Op];
        a[Op] = parent.a[My];
    }

    void swap() {
        std::swap(a[My], a[Op]);
    }

    void drop(Side::_t si, PieceType ty, Square to) {
        a[si].drop(My, ty, to);
        a[~si].drop(Op, ty, ~to);
    }

    void move(PieceType::_t ty, Square from, Square to) {
        a[Op].move(My, ty, from, to);
        a[My].move(Op, ty, ~from, ~to);
    }

    void move(PieceType::_t ty, Square from, Square to, NonKingType::_t captured) {
        a[Op].move(My, ty, from, to, captured);
        a[My].move(Op, ty, ~from, ~to, captured);
    }

    void promote(PromoType promoted, Square from, Square to) {
        a[Op].promote(My, promoted, from, to);
        a[My].promote(Op, promoted, ~from, ~to);
    }

    void promote(PromoType promoted, Square from, Square to, NonKingType::_t captured) {
        a[Op].promote(My, promoted, from, to, captured);
        a[My].promote(Op, promoted, ~from, ~to, captured);
    }

    void ep(Square from, Square to, Square capture) {
        a[Op].ep(My, from, to, capture);
        a[My].ep(Op, ~from, ~to, ~capture);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        a[Op].castle(My, kingFrom, kingTo, rookFrom, rookTo);
        a[My].castle(Op, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
