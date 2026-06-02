#ifndef NNUE_HPP
#define NNUE_HPP

#ifdef __AVX2__
    #define USE_AVX2 1
    #include <immintrin.h>
#else
    #define USE_AVX2 0
#endif

#include <bit>
#include "Index.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));
using vi32x8_t  = i32_t __attribute__((vector_size(32)));

constexpr vi16x16_t vi16x16x(i16_t e) { return vi16x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }

template <typename V>
constexpr V max(V a, V b) {
#ifdef __clang__
    return __builtin_elementwise_max(a, b);
#else
    return a > b ? a : b;
#endif
}

template <typename V>
constexpr V min(V a, V b) {
#ifdef __clang__
    return __builtin_elementwise_min(a, b);
#else
    return a < b ? a : b;
#endif
}

template <typename V>
constexpr V clamp(V a, V b, V c) {
    return min(max(a, b), c);
}

/** NNUE evaluation. Net architecture and constants make it
 * compatible with the simple example provided by the bullet trainer:
 * https://github.com/jw1912/bullet/blob/main/examples/simple.rs
 *
 * Actual training script ../net/simple.rs
 **/

// (768 -> 128) x 2 -> 1
struct CACHE_ALIGN Nnue {
    using _t = vi16x16_t;
    static constexpr int VECTOR_SIZE = sizeof(_t) / sizeof(i16_t);
    static constexpr int HIDDEN_SIZE = 128; // 2*128 = (8*32) = 256 bytes
    static constexpr int SCALE = 400;
    static constexpr int QA = 256;
    static constexpr int QB = 64;

    struct FeatureIndex : ::Index<FeatureIndex, 768> {
        using Index::Index;

        static constexpr array<PieceType::_t, PieceType> pieceType = {Pawn, Knight, Bishop, Rook, Queen, King};

        //TODO: reshape feauture indexing during net loading
        constexpr FeatureIndex (Side si, PieceType ty, Square sq)
            : Index{ 6*64*+si + 64*pieceType[ty] + +~sq }
        {}
    };

    struct AccumulatorSideIndex : Index<AccumulatorSideIndex, HIDDEN_SIZE / VECTOR_SIZE> { using Index::Index; };
    struct AccumulatorIndex : Index<AccumulatorIndex, 2*AccumulatorSideIndex::size()> { using Index::Index; };

    using L0b = array<_t, AccumulatorSideIndex>;
    using L0w = array<L0b, FeatureIndex>;
    using L1w = array<_t, AccumulatorIndex>;

    L0w l0w;   // 768*(8*32) = 196608 bytes
    L0b l0b;   // (8*32) = 256 bytes
    L1w l1w;   // 2*(8*32) = 512 bytes
    i16_t l1b; // 64 aligned bytes, total = 197440 bytes

    // raw NNUE static evaluation
    constexpr i32_t evaluate(const L1w& accumulator) {
#if USE_AVX2
        vi32x8_t add{0};
        for (auto i : range<AccumulatorIndex>()) {
            auto v = clamp(accumulator[i], vi16x16x(0), vi16x16x(QA-1)); // CReLU
            auto vw = v * l1w[i];
            add += static_cast<vi32x8_t>(_mm256_madd_epi16(v, vw)); //SCReLU
        }

#ifdef __clang__
        auto sum = __builtin_reduce_add(add);
#else
        auto sum = add[0] + add[1] + add[2] + add[3] + add[4] + add[5] + add[6] + add[7];
#endif
        sum += static_cast<i64_t>(l1b) * QA;
        return static_cast<i32_t>((static_cast<i64_t>(sum) * SCALE) / (QA * QA * QB));
#else
        int64_t sum{0};
        for (auto i : range<AccumulatorIndex>()) {
            auto v = clamp(accumulator[i], vi16x16x(0), vi16x16x(QA-1)); // CReLU
            auto vw = v * l1w[i];

            for (int j = 0; j < 16; ++j) {
                sum += static_cast<i64_t>(v[j]) * static_cast<i64_t>(vw[j]);
            }
        }
        sum += static_cast<i64_t>(l1b) * QA;
        return static_cast<i32_t>((sum * SCALE) / (QA * QA * QB));
#endif
    }

    // load from embedded binary data, defined in main.cpp
    void setEmbeddedEval();
};

extern Nnue nnue;

// 2x128 neurons, 512 bytes
class CACHE_ALIGN Accumulator {
    // 128 neurons, 256 bytes
    class AccumulatorSide {
        using Index = Nnue::AccumulatorSideIndex; // 8
        using Fi = Nnue::FeatureIndex; // 768
        using _t = Nnue::_t; // vi16x16_t

        static constexpr auto& w = nnue.l0w; // feauture weights
        static constexpr auto& b = nnue.l0b; // feauture biases

        array<_t, Index> v_;

        constexpr void move(Index i, Side si, PieceType ty, Square from, Square to) {
            v_[i] -= w[Fi{si, ty, from}][i];
            v_[i] += w[Fi{si, ty, to}][i];
        }

        constexpr void promote(Index i, Side si, Square from, PromoType promoted, Square to) {
            v_[i] -= w[Fi{si, Pawn, from}][i];
            v_[i] += w[Fi{si, promoted, to}][i];
        }

        constexpr void capture(Index i, Side si, NonKingType captured, Square to) {
            v_[i] -= w[Fi{~si, captured, to}][i];
        }

    public:
        constexpr AccumulatorSide() {
            for (auto i : range<Index>()) {
                v_[i] = b[i];
            }
        }

        constexpr void drop(Side si, PieceType ty, Square to) {
            for (auto i : range<Index>()) {
                v_[i] += w[Fi{si, ty, to}][i];
            }
        }

        constexpr void move(Side si, PieceType ty, Square from, Square to) {
            for (auto i : range<Index>()) {
                move(i, si, ty, from, to);
            }
        }

        constexpr void move(Side si, PieceType ty, Square from, Square to, NonKingType captured) {
            for (auto i : range<Index>()) {
                move(i, si, ty, from, to);
                capture(i, si, captured, to);
            }
        }

        constexpr void promote(Side si, Square from, PromoType promoted, Square to) {
            for (auto i : range<Index>()) {
                promote(i, si, from, promoted, to);
            }
        }

        constexpr void promote(Side si, Square from, PromoType promoted, Square to, NonKingType captured) {
            for (auto i : range<Index>()) {
                promote(i, si, from, promoted, to);
                capture(i, si, captured, to);
            }
        }

        constexpr void ep(Side si, Square from, Square to, Square ep) {
            for (auto i : range<Index>()) {
                move(i, si, Pawn, from, to);
                capture(i, si, NonKingType{Pawn}, ep);
            }
        }

        constexpr void castle(Side si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
            for (auto i : range<Index>()) {
                move(i, si, King, kingFrom, kingTo);
                move(i, si, Rook, rookFrom, rookTo);
            }
        }
    };

    union {
        array<AccumulatorSide, Side> side;
        Nnue::L1w accumulator;
    };
    static_assert (sizeof(side) == sizeof(accumulator));

public:
    // raw NNUE static evaluation
    constexpr auto evaluate() const {
        return nnue.evaluate(accumulator);
    }

    constexpr Accumulator () {
        side[Side{My}] = {};
        side[Side{Op}] = {};
    }

    // copy parent accumulator but flip sides
    constexpr void flip(const Accumulator& parent) {
        side[Side{My}] = parent.side[Side{Op}];
        side[Side{Op}] = parent.side[Side{My}];
    }

    constexpr void swap() {
        std::swap(side[Side{My}], side[Side{Op}]);
    }

    constexpr void drop(Side si, PieceType ty, Square to) {
        side[si].drop(Side{My}, ty, to);
        side[~si].drop(Side{Op}, ty, ~to);
    }

    constexpr void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        side[Side{Op}].move(Side{My}, ty, from, to);
        side[Side{My}].move(Side{Op}, ty, ~from, ~to);
    }

    constexpr void move(PieceType ty, Square from, Square to, NonKingType captured) {
        assert (from != to);
        side[Side{Op}].move(Side{My}, ty, from, to, captured);
        side[Side{My}].move(Side{Op}, ty, ~from, ~to, captured);
    }

    constexpr void promote(Square from, PromoType promoted, Square to) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Side{Op}].promote(Side{My}, from, promoted, to);
        side[Side{My}].promote(Side{Op}, ~from, promoted, ~to);
    }

    constexpr void promote(Square from, PromoType promoted, Square to, NonKingType captured) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Side{Op}].promote(Side{My}, from, promoted, to, captured);
        side[Side{My}].promote(Side{Op}, ~from, promoted, ~to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5));
        assert (to.on(Rank6));
        assert (ep.on(Rank5));
        side[Side{Op}].ep(Side{My}, from, to, ep);
        side[Side{My}].ep(Side{Op}, ~from, ~to, ~ep);
    }

    constexpr void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom.on(Rank1));
        assert (rookTo.on(Rank1));
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);
        side[Side{Op}].castle(Side{My}, kingFrom, kingTo, rookFrom, rookTo);
        side[Side{My}].castle(Side{Op}, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
