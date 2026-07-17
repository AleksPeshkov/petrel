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

template <typename V>
constexpr V abs(V v) {
#ifdef __clang__
    return __builtin_elementwise_abs(v);
#else
    return v < 0 ? -v : v;
#endif
}

constexpr i32x8_t madd16(i16x16_t w, i16x16_t v) {
#if USE_AVX2
    return static_cast<i32x8_t>(_mm256_madd_epi16(w, v));
#else
    // _mm256_madd_epi16 emulation
    i32x8_t sum{};
    for (int i = 0; i < 8; ++i) {
        sum[i] = static_cast<i32_t>(w[2*i]) * static_cast<i32_t>(v[2*i])
            + static_cast<i32_t>(w[2*i+1]) * static_cast<i32_t>(v[2*i+1]);
    }
    return sum;
#endif
}

// (768 -> 128) x 2 -> 1
struct CACHE_ALIGN Nnue {
    using _t = i16x16_t;
    static constexpr int VECTOR_SIZE = sizeof(_t) / sizeof(i16_t); // 16 elements
    static constexpr int ACC_SIZE = 128; // 2*128 = (8*32) = 256 bytes

    struct FeatureIndex : ::Index<FeatureIndex, 768> {
        using Index::Index;

        static constexpr array<PieceType::_t, PieceType> pieceType = {Pawn, Knight, Bishop, Rook, Queen, King};

        //TODO: reshape feature indexing during net loading
        constexpr FeatureIndex (Side si, PieceType ty, Square sq)
            : Index{ 6*64*+si + 64*pieceType[ty] + (+sq ^ 077) }
        {}
    };

    struct AccIndex : Index<AccIndex, ACC_SIZE / VECTOR_SIZE> { using Index::Index; };
    struct AccTwinIndex : Index<AccTwinIndex, 2*AccIndex::size()> { using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, AccTwinIndex>;

    W0 w0; // feature weights, 768*(8*32) = 196608 bytes, feature biases = 0
    W1 w1; // accumulator weights, 2*(8*32) = 512 bytes, accumulator bias = 0

    static constexpr int QUANT = 181; // activation function limits

    // PolyTanh inference activation function: f(x) = x * (2 - |x|)
    static constexpr i16x16_t polyTanh(i16x16_t x) {
        auto c = clamp((x + 2) >> 2, i16x16x(-QUANT), i16x16x(QUANT));
        return c * (2*QUANT - abs(c));
    }

    template <size_t BlockIdx, size_t... Is>
    static constexpr i32x8_t madd4(const W1& w, const W1& v, std::index_sequence<Is...>) {
        constexpr size_t START_IDX = BlockIdx * 4;
        return ( ... + madd16(w[AccTwinIndex{START_IDX + Is}], polyTanh(v[AccTwinIndex{START_IDX + Is}])) );
    }

    template <size_t SHIFT, size_t... BlockIndices>
    static constexpr i32x8_t addScaled(const W1& w, const W1& v, std::index_sequence<BlockIndices...>) {
        constexpr int32_t ROUND_BIAS = 1 << (SHIFT - 1);
        return ( ... + ((madd4<BlockIndices>(w, v, std::make_index_sequence<4>{}) + ROUND_BIAS) >> SHIFT) );
    }

    int32_t evaluate(const W1& acc) {
        constexpr size_t TOTAL_MADD = AccTwinIndex::size();
        constexpr size_t BLOCK_SIZE = 4; // safe when SCALE_LOG = 4,
        constexpr size_t NUM_BLOCKS = TOTAL_MADD / BLOCK_SIZE; // 1 for 64, 2 for 128, 4 for 256 ...
        constexpr size_t BLOCK_SHIFT = (std::bit_width(NUM_BLOCKS) - 1) + 3;

        i32x8_t sum8 = addScaled<BLOCK_SHIFT>(this->w1, acc, std::make_index_sequence<NUM_BLOCKS>{});

        #ifdef __clang__
            i32_t sum = __builtin_reduce_add(sum8);
        #else
            i32_t sum = sum8[0] + sum8[1] + sum8[2] + sum8[3] + sum8[4] + sum8[5] + sum8[6] + sum8[7];
        #endif

        constexpr unsigned QUANT_LOG = 15; // (1 << 15) = 32768 ~= 32761 (QUANT * QUANT)
        constexpr unsigned SCALE_LOG = 4; // extra W1 weights quantization

        constexpr auto SHIFT = QUANT_LOG + SCALE_LOG - BLOCK_SHIFT;
        constexpr auto ROUND_BIAS = 1 << (SHIFT - 1);
        auto result = (sum + ROUND_BIAS) >> SHIFT; // symmetric signed rounding towards zero
        return result;
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

    constexpr void add(Nnue::FeatureIndex fi) {
        for (auto i : range<Index>()) {
            #if USE_AVX2
                v_[i] = _mm256_adds_epi16(v_[i], nnue.w0[fi][i]);
            #else
                v_[i] += nnue.w0[fi][i];
            #endif
        }
    }

    constexpr void sub(Nnue::FeatureIndex fi) {
        for (auto i : range<Index>()) {
            #if USE_AVX2
                v_[i] = _mm256_subs_epi16(v_[i], nnue.w0[fi][i]);
            #else
                v_[i] -= nnue.w0[fi][i];
            #endif
        }
    }

public:
    constexpr Acc() : v_{} {} // init feature biases, none for current net

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
