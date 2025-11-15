#ifndef NNUE_HPP
#define NNUE_HPP

#include "typedefs.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));

// Made compatible with net files made for
// Publius chess engine by Pawel Koziol

// NNUE evaluation. Net architecture and constants make it
// equivalent to the simple example provided by the bullet trainer:
// https://github.com/jw1912/bullet/blob/main/examples/simple.rs
// The architecture is (768 -> HIDDEN_SIZE)x2 -> 1

struct alignas(64) Nnue {
    static constexpr int HIDDEN_SIZE = 32;

    using IndexL0 = Index<768>;
    using IndexL1 = Index<HIDDEN_SIZE/16>;
    using IndexL2 = Index<2*IndexL1::Size>;

    IndexL0::arrayOf<IndexL1::arrayOf<vi16x16_t>> inputWeights;
    IndexL1::arrayOf<vi16x16_t> hiddenBiases;
    IndexL2::arrayOf<vi16x16_t> outputWeights;
    i16_t outputBias;
};

extern Nnue nnue;

class alignas(64) Accumulator {
    //TODO: reshape input weights during net loading
    constexpr static Nnue::IndexL0 indexL0(Side::_t si, PieceType::_t ty, Square sq) {
        constexpr int pieceType[6] = {4, 3, 2, 1, 0, 5};
        return Nnue::IndexL0{ 6*64*(si) + 64*pieceType[ty] + (~sq) };
    }

    using _t = Nnue::IndexL1::arrayOf<vi16x16_t>;
    _t v;

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
            v[i] -= w[indexL0(si, Pawn, capture)][i];
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

#endif
