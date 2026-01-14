#ifndef NNUE_HPP
#define NNUE_HPP

#include <bit>
#include "Index.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));
using vi32x8_t  = i32_t __attribute__((vector_size(32)));

#ifdef __AVX2__
    #define HAS_AVX2 1
    #include <immintrin.h>

    inline vi32x8_t madd(vi16x16_t a, vi16x16_t b) {
        return _mm256_madd_epi16(a, b);
    }
#else
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

template <typename A, typename B>
constexpr A max(A a, B b) {
    return (a & (a >= b)) | (b & (a < b));
}

template <typename A, typename B>
constexpr A min(A a, B b) {
    return (a & (a < b)) | (b & (a >= b));
}

template <typename V1, typename V2, typename V3>
constexpr V1 clamp(V1 a, V2 b, V3 c) {
    return min(max(a, b), c);
}

/** NNUE evaluation. Net architecture and constants make it
 * compatible with the simple example provided by the bullet trainer:
 * https://github.com/jw1912/bullet/blob/main/examples/simple.rs
 *
 * Actual training script ../net/petrel128.rs
 **/

// (768 -> 128) x 2 -> 1
struct CACHE_ALIGN Nnue {
    using _t = vi16x16_t;
    static constexpr int VECTOR_SIZE = sizeof(_t) / sizeof(i16_t);
    static constexpr int HIDDEN_SIZE = 128; // 2*128 = (8*32) = 256 bytes
    static constexpr int SCALE = 400;
    static constexpr int QA = 255;
    static constexpr int QB = 22;

    using FeautureIndex = Index<768>;
    using HalfAccumulatorIndex = Index<HIDDEN_SIZE / VECTOR_SIZE>;
    using AccumulatorIndex = Index<2*HalfAccumulatorIndex::Size>;

    using L0b = HalfAccumulatorIndex::arrayOf<_t>;
    using L0w = FeautureIndex::arrayOf<L0b>;
    using L1w = AccumulatorIndex::arrayOf<_t>;

    L0w l0w;   // 768*(8*32) = 196608 bytes
    L0b l0b;   // (8*32) = 256 bytes
    L1w l1w;   // 2*(8*32) = 512 bytes
    i16_t l1b; // 64 aligned bytes, total = 197440 bytes

    // SCReLU activation function
    constexpr i32_t evaluate(const L1w& accumulator) {
        vi32x8_t sum{0};

        for (auto i : range<AccumulatorIndex>()) {
            auto v16 = clamp(accumulator[i], static_cast<i16_t>(0), static_cast<i16_t>(QA)); // CReLU
            sum += madd(v16, v16 * l1w[i]); //SCReLU
        }

        i32_t output = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
        return (output / QA + l1b) * SCALE / (QA * QB);
    }

    // load from embedded binary data, defined in main.cpp
    void setEmbeddedEval();
};

extern Nnue nnue;

// 2x128 neurons, 512 bytes
//TRICK: accumulator assumes updates from NOT side to move
class CACHE_ALIGN Accumulator {
    // 128 neurons, 256 bytes
    class AccumulatorSide {
        using Index = Nnue::HalfAccumulatorIndex; // 8

        constexpr static auto& w = nnue.l0w; // feauture weights
        constexpr static auto& b = nnue.l0b; // feauture biases

        //TODO: reshape feauture indexing during net loading
        constexpr static Nnue::FeautureIndex fi(Side::_t si, PieceType::_t ty, Square sq) {
            constexpr int pieceType[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
            return Nnue::FeautureIndex{ 6*64*(si) + 64*pieceType[ty] + (~sq) };
        }

        constexpr void move(Index i, Side::_t si, PieceType::_t fromType, Square from, PieceType::_t toType, Square to) {
            v[i] -= w[fi(si, fromType, from)][i];
            v[i] += w[fi(si, toType, to)][i];
        }

        constexpr void capture(Index i, Side::_t si, PieceType::_t captured, Square to) {
            v[i] -= w[fi(~si, captured, to)][i];
        }

        Index::arrayOf<Nnue::_t> v;

    public:
        constexpr void clear() {
            for (auto i : range<Index>()) {
                v[i] = b[i];
            }
        }

        void drop(Side::_t si, PieceType ty, Square to) {
            for (auto i : range<Index>()) {
                v[i] += w[fi(si, ty, to)][i];
            }
        }

        void move(Side::_t si, PieceType ty, Square from, Square to) {
            for (auto i : range<Index>()) {
                move(i, si, ty, from, ty, to);
            }
        }

        void move(Side::_t si, PieceType ty, Square from, Square to, NonKingType captured) {
            for (auto i : range<Index>()) {
                move(i, si, ty, from, ty, to);
                capture(i, si, captured, to);
            }
        }

        void promote(Side::_t si, PromoType promoted, Square from, Square to) {
            for (auto i : range<Index>()) {
                move(i, si, Pawn, from, promoted, to);
            }
        }

        void promote(Side::_t si, PromoType promoted, Square from, Square to, NonKingType captured) {
            for (auto i : range<Index>()) {
                move(i, si, Pawn, from, promoted, to);
                capture(i, si, captured, to);
            }
        }

        void ep(Side::_t si, Square from, Square to, Square ep) {
            for (auto i : range<Index>()) {
                move(i, si, Pawn, from, Pawn, to);
                capture(i, si, Pawn, ep);
            }
        }

        void castle(Side::_t si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
            for (auto i : range<Index>()) {
                move(i, si, King, kingFrom, King, kingTo);
                move(i, si, Rook, rookFrom, Rook, rookTo);
            }
        }
    };

    union {
        Side::arrayOf<AccumulatorSide> side;
        Nnue::L1w accumulator;
    };
    static_assert(sizeof(side) == sizeof(accumulator));

public:
    auto evaluate() const {
        return nnue.evaluate(accumulator);
    }

    void clear() {
        side[Side{My}].clear();
        side[Side{Op}].clear();
    }

    void copyParent(const Accumulator& parent) {
        side[Side{My}] = parent.side[Side{Op}];
        side[Side{Op}] = parent.side[Side{My}];
    }

    void swap() {
        std::swap(side[Side{My}], side[Side{Op}]);
    }

    void drop(Side si, PieceType ty, Square to) {
        side[si].drop(Side{My}, ty, to);
        side[~si].drop(Side{Op}, ty, ~to);
    }

    void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        side[Side{Op}].move(Side{My}, ty, from, to);
        side[Side{My}].move(Side{Op}, ty, ~from, ~to);
    }

    void move(PieceType ty, Square from, Square to, NonKingType captured) {
        assert (from != to);
        side[Side{Op}].move(Side{My}, ty, from, to, captured);
        side[Side{My}].move(Side{Op}, ty, ~from, ~to, captured);
    }

    void promote(PromoType promoted, Square from, Square to) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Side{Op}].promote(Side{My}, promoted, from, to);
        side[Side{My}].promote(Side{Op}, promoted, ~from, ~to);
    }

    void promote(PromoType promoted, Square from, Square to, NonKingType captured) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Side{Op}].promote(Side{My}, promoted, from, to, captured);
        side[Side{My}].promote(Side{Op}, promoted, ~from, ~to, captured);
    }

    void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5));
        assert (to.on(Rank6));
        assert (ep.on(Rank5));
        side[Side{Op}].ep(Side{My}, from, to, ep);
        side[Side{My}].ep(Side{Op}, ~from, ~to, ~ep);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom.on(Rank1));
        assert (rookTo.on(Rank1));
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);
        side[Side{Op}].castle(Side{My}, kingFrom, kingTo, rookFrom, rookTo);
        side[Side{My}].castle(Side{Op}, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
