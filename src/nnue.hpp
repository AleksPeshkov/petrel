#ifndef NNUE_HPP
#define NNUE_HPP

#include "bitops256.hpp"
#include "Index.hpp"

using i16x16_t = i16_t __attribute__((vector_size(32)));
using i32x8_t  = i32_t __attribute__((vector_size(32)));

constexpr i16x16_t i16x16x(i16_t e) { return i16x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }

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
    using _t = i16x16_t;
    static constexpr int VECTOR_SIZE = sizeof(_t) / sizeof(i16_t);
    static constexpr int ACC_SIZE = 128; // 2*128 = (8*32) = 256 bytes
    static constexpr int SCALE = 400;
    static constexpr int QA = 256;
    static constexpr int QB = 64;

    struct FeatureIndex : ::Index<FeatureIndex, 768> {
        using Index::Index;

        static constexpr array<PieceType::_t, PieceType> pieceType = {Pawn, Knight, Bishop, Rook, Queen, King};

        //TODO: reshape feature indexing during net loading
        constexpr FeatureIndex (Side si, PieceType ty, Square sq)
            : Index{ 6*64*+si + 64*pieceType[ty] + +~sq }
        {}
    };

    struct AccIndex : Index<AccIndex, ACC_SIZE / VECTOR_SIZE> { using Index::Index; };
    struct AccTwinIndex : Index<AccTwinIndex, 2*AccIndex::size()> { using Index::Index; };

    using B0 = array<_t, AccIndex>;
    using W0 = array<B0, FeatureIndex>;
    using W1 = array<_t, AccTwinIndex>;

    W0 w0;    // feature weights, 768*(8*32) = 196608 bytes
    B0 b0;    // feature biases, (8*32) = 256 bytes
    W1 w1;    // accumulator weights, 2*(8*32) = 512 bytes
    i16_t b1; // accumulator bias, 64 aligned bytes, total = 197440 bytes

    // raw NNUE static evaluation
    constexpr i32_t evaluate(const W1& acc) {
#if USE_AVX2
        i32x8_t sum8{0};
        for (auto i : range<AccTwinIndex>()) {
            auto v = clamp(acc[i], i16x16x(0), i16x16x(QA-1)); // CReLU
            auto vw = v * w1[i];
            sum8 += static_cast<i32x8_t>(_mm256_madd_epi16(v, vw)); //SCReLU
        }

    #ifdef __clang__
        auto sum = __builtin_reduce_add(sum8);
    #else
        auto sum = sum8[0] + sum8[1] + sum8[2] + sum8[3] + sum8[4] + sum8[5] + sum8[6] + sum8[7];
    #endif

#else
        int32_t sum{0};
        for (auto i : range<AccTwinIndex>()) {
            auto v = clamp(acc[i], i16x16x(0), i16x16x(QA-1)); // CReLU
            auto vw = v * w1[i];

            for (int j = 0; j < 16; ++j) {
                sum += static_cast<i32_t>(v[j]) * static_cast<i32_t>(vw[j]);
            }
        }
#endif

        sum += static_cast<i32_t>(b1) * QA;
        return static_cast<i32_t>((static_cast<i64_t>(sum) * SCALE) / (QA * QA * QB));
    }

    // load from embedded binary data, defined in main.cpp
    void setEmbeddedEval();
};

extern Nnue nnue;

// 128 neurons, 256 bytes
class CACHE_ALIGN Acc {
    using Index = Nnue::AccIndex;
    using _t = array<Nnue::_t, Index>; // i16x16_t[8]
    _t v_;

    constexpr void add(Nnue::FeatureIndex fi) { for (auto i : range<Index>()) { v_[i] += nnue.w0[fi][i]; } }
    constexpr void sub(Nnue::FeatureIndex fi) { for (auto i : range<Index>()) { v_[i] -= nnue.w0[fi][i]; } }

public:
    constexpr Acc() : v_{nnue.b0} {} // feature biases

    static constexpr void flip(Acc& my, Acc& op) {
        for (auto i : range<Index>()) { std::swap(my.v_[i], op.v_[i]); }
    }

    constexpr void drop(Side si, PieceType ty, Square to) {
        add({ si, ty, to });
    }

    constexpr void move(Side si, PieceType ty, Square from, Square to) {
        sub({ si, ty, from });
        add({ si, ty, to });
    }

    constexpr void castle(Side si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        move(si, King, kingFrom, kingTo);
        move(si, Rook, rookFrom, rookTo);
    }

    constexpr void move(Side si, PieceType ty, Square from, Square to, NonKingType captured) {
        move(si, ty, from, to);
        sub({~si, captured, to});
    }

    constexpr void ep(Side si, Square from, Square to, Square ep) {
        move(si, Pawn, from, to);
        sub({~si, Pawn, ep});
    }

    constexpr void promote(Side si, Square from, PromoType promoted, Square to) {
        sub({si, Pawn, from});
        add({si, promoted, to});
    }

    constexpr void promote(Side si, Square from, PromoType promoted, Square to, NonKingType captured) {
        promote(si, from, promoted, to);
        sub({~si, captured, to});
    }
};

// 2x128 neurons, 512 bytes
class AccTwin {
    array<Acc, Side> side;
public:
    // raw NNUE static evaluation
    constexpr auto evaluate() const { return nnue.evaluate(std::bit_cast<Nnue::W1>(side)); }

    constexpr AccTwin () : side{} {}

    // copy parent accumulator but flip sides
    constexpr void flip(const AccTwin& parent) {
        side[Side{My}] = parent.side[Side{Op}];
        side[Side{Op}] = parent.side[Side{My}];
    }

    constexpr void flip() { Acc::flip(side[Side{My}], side[Side{Op}]); }

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
