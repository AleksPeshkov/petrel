#ifndef NNUE_HPP
#define NNUE_HPP

#include "bitops256.hpp"
#include "Index.hpp"

using i16x16_t = i16_t __attribute__((vector_size(32)));
using u16x16_t = u16_t __attribute__((vector_size(32)));
using i32x8_t  = i32_t __attribute__((vector_size(32)));
using i64x4_t  = i64_t __attribute__((vector_size(32)));

constexpr i16x16_t i16x16x(i16_t e) { return i16x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }
constexpr u16x16_t u16x16x(u16_t e) { return u16x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }

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

constexpr i16x16_t clamp(i16x16_t a, int b, int c) {
    return min(max(a, i16x16x(b)), i16x16x(c));
}

template <typename V>
constexpr V abs(V v) {
    #ifdef __clang__
        return __builtin_elementwise_abs(v);
    #else
        return v < 0 ? -v : v;
    #endif
}

constexpr u16x16_t mulhi_u16(u16x16_t a, u16x16_t b) {
    #if USE_AVX2
        return _mm256_mulhi_epu16(a, b);
    #else
        u16x16_t res{};
        for (int i = 0; i < 16; ++i) {
            res[i] = static_cast<u16_t>((static_cast<u32_t>(a[i]) * static_cast<u32_t>(b[i])) >> 16);
        }
        return res;
    #endif
}

constexpr i16x16_t sign_i16(u16x16_t x, i16x16_t sign) {
    #if USE_AVX2
        return _mm256_sign_epi16(x, sign);
    #else
        return sign < 0 ? -x : x;
    #endif
}

inline i64x4_t unpack_add_i32(i32x8_t a) {
    // signed extension from i32 to i64
    i64x4_t low = __builtin_convertvector(__builtin_shufflevector(a, a, 0, 1, 2, 3), i64x4_t);
    i64x4_t high = __builtin_convertvector(__builtin_shufflevector(a, a, 4, 5, 6, 7), i64x4_t);
    return low + high;
}

inline i64_t hadd_i64(i64x4_t sum4) {
    #ifdef __clang__
        return __builtin_reduce_add(sum4);
    #else
        return sum4[0] + sum4[1] + sum4[2] + sum4[3];
    #endif
}

constexpr i32x8_t madd_i16(i16x16_t w, i16x16_t v) {
    #if USE_AVX2
        return _mm256_madd_epi16(w, v);
    #else
        i32x8_t sum{};
        for (int i = 0; i < 8; ++i) {
            sum[i] = static_cast<i32_t>(w[2*i]) * static_cast<i32_t>(v[2*i])
                + static_cast<i32_t>(w[2*i+1]) * static_cast<i32_t>(v[2*i+1]);
        }
        return sum;
    #endif
}

struct CACHE_ALIGN Nnue {
    struct FeatureIndex : ::Index<FeatureIndex, 2*6*64> { using Index::Index;
        constexpr FeatureIndex (Side si, PieceType ty, Square sq)
            : Index{ (+si * 6*64) + (+ty * 64) + (+sq) }
        {}
    };

    using _t = i16x16_t;
    static constexpr int Vector_size = sizeof(_t) / sizeof(i16_t);
    static constexpr int Acc_neurons = 128;

    struct AccIndex : Index<AccIndex, Acc_neurons / Vector_size> { using Index::Index; };
    struct AccTwinIndex : Index<AccTwinIndex, 2*AccIndex::size()> { using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, AccTwinIndex>;

    W0 w0;    // feature weights, 768*(8*32) = 196608 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 2*(8*32) = 512 bytes
    i64_t b1; // output bias, total = 197184 bytes

    static constexpr u16x16_t squared(u16x16_t x1024) {
        auto x2 = x1024 << 5; // 2*x [0 .. 32768]
        return mulhi_u16(x2+1, x2); // x*x [0 .. 16384]
    }

    static i32x8_t forward(i16x16_t x, i16x16_t w) {
        auto x1024 = clamp(x, 0, 1024);
        auto activated = squared(x1024); // [0 .. 16384]
        return madd_i16(activated, w);
    }

    using AccTwin = array<_t, AccTwinIndex>;
    int32_t evaluate(const AccTwin& acc) const {
        i64x4_t sum4{};
        for (auto i : range<AccTwinIndex>()) {
            i32x8_t sum8 = forward(acc[i], this->w1[i]);
            sum4 += unpack_add_i32(sum8);
        }
        i64_t output = this->b1 + hadd_i64(sum4);

        constexpr auto Scale = 18; // 4+14 (QB=16, squared() x16384)
        auto result = output >> Scale;
        return result;
    }

    static COLD void validate_embedded_size();
};

extern constinit const Nnue& nnue;

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
    constexpr Acc() : v_{} {} // feature biases = 0

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
    auto evaluate() const { return nnue.evaluate(std::bit_cast<Nnue::AccTwin>(side)); }

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
