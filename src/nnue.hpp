#ifndef NNUE_HPP
#define NNUE_HPP

#include "bitops256.hpp"
#include "Index.hpp"

using i16x16_t = i16_t __attribute__((vector_size(32)));
using u16x16_t = u16_t __attribute__((vector_size(32)));
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

constexpr i16x16_t clamp(i16x16_t a, int b, int c) {
    return min(max(a, i16x16x(b)), i16x16x(c));
}

inline u16x16_t mulhi_u16(u16x16_t a, u16x16_t b) {
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

inline i32_t hadd_i32(i32x8_t sum8) {
    #ifdef __clang__
        return __builtin_reduce_add(sum8);
    #else
        return sum8[0] + sum8[1] + sum8[2] + sum8[3] + sum8[4] + sum8[5] + sum8[6] + sum8[7];
    #endif
}

inline i32x8_t madd_i16(i16x16_t w, i16x16_t v) {
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
    static constexpr int Acc_neurons = 1024;

    struct AccIndex : Index<AccIndex, Acc_neurons / Vector_size> { using Index::Index; };
    struct AccTwinIndex : Index<AccTwinIndex, 2*AccIndex::size()> { using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, AccTwinIndex>;

    W0 w0;    // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 2*(64*32) = 4096 bytes
    i32_t b1; // output bias (64 byte aligned), total = 1577024 bytes

    static constexpr u16x16_t squared(u16x16_t x1024) {
        auto x = x1024 << 4; // [0 .. 16384]
        return mulhi_u16(x+1, x); // x*x [0 .. 4096]
    }

    static i32x8_t forward(i16x16_t x, i16x16_t w) {
        auto x1024 = clamp(x, 0, 1024);
        auto activated = squared(x1024); // [0 .. 4096]
        return madd_i16(activated, w); // w = [-8191,+8191]
    }

    using AccTwin = array<_t, AccTwinIndex>;
    int32_t evaluate(const AccTwin& acc) const {
        i32x8_t sum8{};
        for (auto i : range<AccTwinIndex>()) {
            // safe for up to 32 additions
            sum8 += forward(acc[i], this->w1[i]);
        }
        i32_t output = this->b1 + hadd_i32(sum8);

        constexpr auto Scale = 15; // 12+3 (squared() x4096, QB=8)
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
        side[My] = parent.side[Op];
        side[Op] = parent.side[My];
    }

    constexpr void flip() { Acc::flip(side[My], side[Op]); }

    constexpr void drop(Side si, PieceType ty, Square to) {
        side[si].drop(My, ty, to);
        side[~si].drop(Op, ty, ~to);
    }

    constexpr void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        side[Op].move(My, ty, from, to);
        side[My].move(Op, ty, ~from, ~to);
    }

    constexpr void move(PieceType ty, Square from, Square to, NonKingType captured) {
        assert (from != to);
        side[Op].move(My, ty, from, to, captured);
        side[My].move(Op, ty, ~from, ~to, captured);
    }

    constexpr void promote(Square from, PromoType promoted, Square to) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Op].promote(My, from, promoted, to);
        side[My].promote(Op, ~from, promoted, ~to);
    }

    constexpr void promote(Square from, PromoType promoted, Square to, NonKingType captured) {
        assert (from.on(Rank7));
        assert (to.on(Rank8));
        side[Op].promote(My, from, promoted, to, captured);
        side[My].promote(Op, ~from, promoted, ~to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5));
        assert (to.on(Rank6));
        assert (ep.on(Rank5));
        side[Op].ep(My, from, to, ep);
        side[My].ep(Op, ~from, ~to, ~ep);
    }

    constexpr void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom.on(Rank1));
        assert (rookTo.on(Rank1));
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);
        side[Op].castle(My, kingFrom, kingTo, rookFrom, rookTo);
        side[My].castle(Op, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
