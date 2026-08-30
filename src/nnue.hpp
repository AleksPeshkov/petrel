#ifndef NNUE_HPP
#define NNUE_HPP

#include "bitops256.hpp"
#include "Index.hpp"

using i16x16_t = i16_t __attribute__((vector_size(32)));
using u16x16_t = u16_t __attribute__((vector_size(32)));
using i32x8_t  = i32_t __attribute__((vector_size(32)));
using i64x4_t  = i64_t __attribute__((vector_size(32)));

constexpr i16x16_t i16x16x(i16_t e) { return i16x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }

inline i16x16_t adds_i16(i16x16_t a, i16x16_t b) {
    #ifdef __clang__
        return __builtin_elementwise_add_sat(a, b);
    #else
        using i32x16_t = i32_t __attribute__((vector_size(64)));

        i32x16_t sum = __builtin_convertvector(a, i32x16_t) + __builtin_convertvector(b, i32x16_t);
        sum = (sum > 32767) ? 32767 : sum;
        sum = (sum < -32768) ? -32768 : sum;

        return __builtin_convertvector(sum, i16x16_t);
    #endif
}

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

constexpr i16x16_t clamp(i16x16_t a, int low, int high) {
    return min(max(a, i16x16x(low)), i16x16x(high));
}

inline i16x16_t mulhrs_i16(i16x16_t a, i16x16_t b) {
    #if USE_AVX2
        return _mm256_mulhrs_epi16(a, b);
    #else
        i16x16_t res{};
        for (int i = 0; i < 16; ++i) {
            auto prod = static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
            res[i] = static_cast<int16_t>((prod + 0x4000) >> 15);
        }
        return res;
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
    struct DualAccIndex : Index<DualAccIndex, 2*AccIndex::size()> { using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, DualAccIndex>;

    W0 w0;    // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 2*(64*32) = 4096 bytes
    i64_t b1; // output bias (64 byte aligned), total = 1577024 bytes

    Nnue ();

    static i32x8_t forward(i16x16_t x, i16x16_t w) {
        auto c = clamp(x, 0, 1024);
        auto cw = mulhrs_i16(c << 4, w);
        return madd_i16(c, cw); // sum of two products
    }

    using DualAcc = array<_t, DualAccIndex>;
    int32_t evaluate(const DualAcc& dual_acc) const {
        i32x8_t sum8{};
        for (auto n : range<DualAccIndex>()) {
            // safe for 64 additions (128 products)
            sum8 += forward(dual_acc[n], this->w1[n]);
        }
        auto sum4 = unpack_add_i32(sum8);
        i64_t output = this->b1 + hadd_i64(sum4);

        constexpr auto Scale = 14; // QA*QA: 2*10, QB: 5, shift: 4, mulhrs_i16: -15
        auto result = output >> Scale;
        return result;
    }
};
extern const Nnue nnue;

class CACHE_ALIGN Acc {
public:
    using Fi = Nnue::FeatureIndex;
    using AccIndex = Nnue::AccIndex;
    using _t = Nnue::_t; // i16x16_t

    static constexpr void swap(Acc& my, Acc& op) {
        for (auto n : range<AccIndex>()) {
            std::swap(my.acc[n], op.acc[n]);
        }
    }

    constexpr void drop(Side si, PieceType ty, Square to) {
        for (auto n : range<AccIndex>()) {
            acc[n] = adds_i16(acc[n], nnue.w0[{si, ty, to}][n]);
        }
    }

    constexpr void move(Side si, PieceType ty, Square from, Square to) {
        move({si, ty, from}, {si, ty, to});
    }

    constexpr void promote(Side si, Square from, PromoType promoted, Square to) {
        move({si, Pawn, from}, {si, promoted, to});
    }

    constexpr void move(Side si, PieceType ty, Square from, Square to, NonKingType captured) {
        capture({si, ty, from}, {si, ty, to}, {~si, captured, to});
    }

    constexpr void promote(Side si, Square from, PromoType promoted, Square to, NonKingType captured) {
        capture({si, Pawn, from}, {si, promoted, to}, {~si, captured, to});
    }

    constexpr void ep(Side si, Square from, Square to, Square ep) {
        capture({si, Pawn, from}, {si, Pawn, to}, {~si, Pawn, ep});
    }

    constexpr void castle(Side si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        for (auto n : range<AccIndex>()) {
            auto s1 = nnue.w0[{si, King, kingTo}][n] - nnue.w0[{si, King, kingFrom}][n];
            auto s2 = nnue.w0[{si, Rook, rookTo}][n] - nnue.w0[{si, Rook, rookFrom}][n];
            acc[n] = adds_i16(acc[n], s1 + s2);
        }
    }

private:
    array<_t, AccIndex> acc{}; // feature biases = 0

    constexpr void move(Fi from, Fi to) {
        for (auto n : range<AccIndex>()) {
            acc[n] = adds_i16(acc[n], nnue.w0[to][n] - nnue.w0[from][n]);
        }
    }

    constexpr void capture(Fi from, Fi to, Fi cap) {
        for (auto n : range<AccIndex>()) {
            acc[n] = adds_i16(acc[n], nnue.w0[to][n] - nnue.w0[from][n] - nnue.w0[cap][n]);
        }
    }
};

class DualAcc {
    array<Acc, Side> side{};
public:
    // raw NNUE static evaluation
    auto evaluate() const { return nnue.evaluate(std::bit_cast<Nnue::DualAcc>(side)); }

    // copy parent accumulator but flip sides
    constexpr void flip(const DualAcc& parent) {
        side[My] = parent.side[Op];
        side[Op] = parent.side[My];
    }

    constexpr void swap() { Acc::swap(side[My], side[Op]); }

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
        assert (from.on(Rank7)); assert (to.on(Rank8));
        side[Op].promote(My, from, promoted, to);
        side[My].promote(Op, ~from, promoted, ~to);
    }

    constexpr void promote(Square from, PromoType promoted, Square to, NonKingType captured) {
        assert (from.on(Rank7)); assert (to.on(Rank8));
        side[Op].promote(My, from, promoted, to, captured);
        side[My].promote(Op, ~from, promoted, ~to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5)); assert (to.on(Rank6)); assert (ep.on(Rank5));
        side[Op].ep(My, from, to, ep);
        side[My].ep(Op, ~from, ~to, ~ep);
    }

    constexpr void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom); assert (kingTo != rookTo);
        assert (kingFrom.on(Rank1)); assert (rookTo.on(Rank1));
        side[Op].castle(My, kingFrom, kingTo, rookFrom, rookTo);
        side[My].castle(Op, ~kingFrom, ~kingTo, ~rookFrom, ~rookTo);
    }
};

#endif
