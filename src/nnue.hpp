#ifndef NNUE_HPP
#define NNUE_HPP

#include "bitops256.hpp"
#include "Index.hpp"

using i16x16_t = i16_t __attribute__((vector_size(32)));
using u16x16_t = u16_t __attribute__((vector_size(32)));
using i32x8_t  = i32_t __attribute__((vector_size(32)));
using i64x4_t  = i64_t __attribute__((vector_size(32)));

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
constexpr V abs(V v) {
    #ifdef __clang__
        return __builtin_elementwise_abs(v);
    #else
        return v < 0 ? -v : v;
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
        constexpr FeatureIndex (Side si, PieceType ty, Square sq, Square mirror)
            : Index{ (+si * 6*64) + (+ty * 64) + +(sq ^ mirror) }
        {}
    };

    using _t = i16x16_t;
    static constexpr int Vector_size = sizeof(_t) / sizeof(i16_t);
    static constexpr int Acc_neurons = 128;

    struct AccIndex : Index<AccIndex, Acc_neurons / Vector_size> { using Index::Index; };
    struct DualAccIndex : Index<DualAccIndex, 2*AccIndex::size()> { using Index::Index; };

    enum activation_enum { Pos, Neg };
    struct HIndex : Index<HIndex, 2, activation_enum>{ using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, HIndex, DualAccIndex>;

    W0 w0;    // feature weights, 768*128*2 = 196608 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 4*128*2 = 1024 bytes
    i64_t b1; // output bias (64 byte aligned), total = 197696 bytes

    static constexpr u16x16_t squared(u16x16_t x1024) {
        auto x2 = x1024 << 5; // 2*x [0 .. 32768]
        return mulhi_u16(x2+1, x2); // x*x [ 0 .. 16384]
    }

    static i32x8_t forward(i16x16_t x, i16x16_t pos, i16x16_t neg) {
        auto xx = squared(clamp(abs(x), 0, 1024));
        // Squared Concatenated ReLU
        return madd_i16(xx, x > 0 ? pos : neg);
    }

    using DualAcc = array<_t, DualAccIndex>;
    int32_t evaluate(const DualAcc& acc) const {
        i64x4_t sum4{};
        for (auto i : range<DualAccIndex>()) {
            auto sum8 = forward(acc[i], this->w1[HIndex{Pos}][i], this->w1[HIndex{Neg}][i]);
            sum4 += unpack_add_i32(sum8);
        }
        i64_t output = this->b1 + hadd_i64(sum4);

        constexpr auto Scale = 18; // 10+4+4 (QA=1024, QB=16, squared=16)
        auto result = output >> Scale;
        return result;
    }

    static COLD void validate_embedded_size();
};

extern constinit const Nnue& nnue;

class Position;

class CACHE_ALIGN Acc {
public:
    using Fi = Nnue::FeatureIndex;
    using AccIndex = Nnue::AccIndex;
    using _t = Nnue::_t; // i16x16_t

    static constexpr void swap(Acc& my, Acc& op) {
        for (auto i : range<AccIndex>()) {
            std::swap(my.acc[i], op.acc[i]);
        }
    }

    // defined in Position.cpp
    template <Side::_t>
    void setup(const Position& pos, Square mirror);

    constexpr void move(Square mirror, Side si, PieceType ty, Square from, Square to) {
        move({si, ty, from, mirror}, {si, ty, to, mirror});
    }

    constexpr void promote(Square mirror, Side si, Square from, PromoType promoted, Square to) {
        move({si, Pawn, from, mirror}, {si, promoted, to, mirror});
    }

    constexpr void move(Square mirror, Side si, PieceType ty, Square from, Square to, NonKingType captured) {
        capture({si, ty, from, mirror}, {si, ty, to, mirror}, {~si, captured, to, mirror});
    }

    constexpr void promote(Square mirror, Side si, Square from, PromoType promoted, Square to, NonKingType captured) {
        capture({si, Pawn, from, mirror}, {si, promoted, to, mirror}, {~si, captured, to, mirror});
    }

    constexpr void ep(Square mirror, Side si, Square from, Square to, Square ep) {
        capture({si, Pawn, from, mirror}, {si, Pawn, to, mirror}, {~si, Pawn, ep, mirror});
    }

    constexpr void castle(Square mirror, Side si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        for (auto i : range<AccIndex>()) {
            auto s1 = nnue.w0[{si, King, kingTo, mirror}][i] - nnue.w0[{si, King, kingFrom, mirror}][i];
            auto s2 = nnue.w0[{si, Rook, rookTo, mirror}][i] - nnue.w0[{si, Rook, rookFrom, mirror}][i];
            #if USE_AVX2
                acc[i] = _mm256_adds_epi16(acc[i], s1 + s2);
            #else
                acc[i] += s1 + s2;
            #endif
        }
    }

private:
    array<_t, AccIndex> acc{}; // feature biases = 0

    constexpr void move(Fi from, Fi to) {
        for (auto i : range<AccIndex>()) {
            #if USE_AVX2
                acc[i] = _mm256_adds_epi16(acc[i], nnue.w0[to][i] - nnue.w0[from][i]);
            #else
                acc[i] += nnue.w0[to][i] - nnue.w0[from][i];
            #endif
        }
    }

    constexpr void capture(Fi from, Fi to, Fi cap) {
        for (auto i : range<AccIndex>()) {
            #if USE_AVX2
                acc[i] = _mm256_adds_epi16(acc[i], nnue.w0[to][i] - nnue.w0[from][i] - nnue.w0[cap][i]);
            #else
                acc[i] += nnue.w0[to][i] - nnue.w0[from][i] - nnue.w0[cap][i];
            #endif
        }
    }
};

class DualAcc {
public:
    using _t = Acc::_t;

    // raw NNUE static evaluation
    auto evaluate() const { return nnue.evaluate(std::bit_cast<Nnue::DualAcc>(side)); }

    // defined in Position.cpp
    void setup(const Position& pos);

    // copy parent accumulator but flip sides
    constexpr void flip(const DualAcc& parent) {
        side[My] = parent.side[Op];
        side[Op] = parent.side[My];
        mirror[My] = parent.mirror[Op];
        mirror[Op] = parent.mirror[My];
    }

    constexpr void swap() {
        Acc::swap(side[My], side[Op]);
        std::swap(mirror[My], mirror[Op]);
    }

    constexpr void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        side[Op].move(mirror[Op], My, ty, from, to);
        side[My].move(~mirror[My], Op, ty, from, to);
    }

    constexpr void move(PieceType ty, Square from, Square to, NonKingType captured) {
        assert (from != to);
        side[Op].move(mirror[Op], My, ty, from, to, captured);
        side[My].move(~mirror[My], Op, ty, from, to, captured);
    }

    constexpr void promote(Square from, PromoType promoted, Square to) {
        assert (from.on(Rank7)); assert (to.on(Rank8));
        side[Op].promote(mirror[Op], My, from, promoted, to);
        side[My].promote(~mirror[My], Op, from, promoted, to);
    }

    constexpr void promote(Square from, PromoType promoted, Square to, NonKingType captured) {
        assert (from.on(Rank7)); assert (to.on(Rank8));
        side[Op].promote(mirror[Op], My, from, promoted, to, captured);
        side[My].promote(~mirror[My], Op, from, promoted, to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5)); assert (to.on(Rank6)); assert (ep.on(Rank5));
        side[Op].ep(mirror[Op], My, from, to, ep);
        side[My].ep(~mirror[My], Op, from, to, ep);
    }

    // defined in Position.cpp
    constexpr void moveKing(const Position&, Square from, Square to);
    constexpr void moveKing(const Position&, Square from, Square to, NonKingType captured);
    constexpr void castle(const Position&, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo);

private:
    array<Acc, Side> side{};
    array<Square, Side> mirror{};
};

#endif
