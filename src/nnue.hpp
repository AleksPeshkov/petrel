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

// NNUE feature layer index
struct Fi : ::Index<Fi, 6*2*64, i16_t> {
    constexpr Fi () : Index{} {}
    constexpr explicit Fi (_t v) : Index{v} {}
    constexpr Fi (Side side, Piece ty, Square sq) : Fi{ static_cast<_t>((+ty*128) + (+side*64) + (+sq)) } {}
    constexpr Fi (Square sqFlip) : Fi{ static_cast<_t>(+sqFlip) } {}

    constexpr Fi operator ^ (Fi mask) const { return Fi{ static_cast<_t>(v_ ^ mask.v_) }; }
    constexpr Fi operator ~ () const { return Fi{ static_cast<_t>(v_ ^ 64) }; } // change side
};

struct CACHE_ALIGN Nnue {
    using _t = i16x16_t;
    static constexpr int Vector_size = sizeof(_t) / sizeof(i16_t);
    static constexpr int Acc_neurons = 1024;

    struct AccIndex : Index<AccIndex, Acc_neurons / Vector_size> { using Index::Index; };
    using Acc = array<_t, AccIndex>;
    using DualAcc = array<Acc, Side>;

    using W0 = array<_t, Fi, AccIndex>;
    using W1 = DualAcc;
    using B1 = i64_t;

    Nnue ();

    static i32x8_t forward(i16x16_t x, i16x16_t w) {
        auto c = clamp(x, 0, 1024);
        auto cw = mulhrs_i16(c << 4, w);
        return madd_i16(c, cw); // sum of two products
    }

    i32_t evaluate(const DualAcc& dacc) const {
        i32x8_t sum8{};
        for (auto side : range<Side>()) {
            for (auto n : range<AccIndex>()) {
                // safe for 64 additions (128 products)
                sum8 += forward(dacc[side][n], this->w1[side][n]);
            }
        }
        i64_t output = this->b1 + hadd_i64(unpack_add_i32(sum8));

        constexpr auto Scale = 14; // QA*QA: 2*10, QB: 5, shift: 4, mulhrs_i16: -15
        auto result = output >> Scale;
        return result;
    }

    constexpr void quiet(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1) const {
        for (auto n : range<AccIndex>()) {
            current[n] = adds_i16(parent[n], this->w0[add1^m][n] - this->w0[sub1^m][n]);
        }
    }

    constexpr void capture(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2) const {
        for (auto n : range<AccIndex>()) {
            current[n] = adds_i16(parent[n], this->w0[add1^m][n] - this->w0[sub1^m][n] - this->w0[sub2^m][n]);
        }
    }

    constexpr void castling(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2, Fi add2) const {
        for (auto n : range<AccIndex>()) {
            auto s1 = this->w0[add1^m][n] - this->w0[sub1^m][n];
            auto s2 = this->w0[add2^m][n] - this->w0[sub2^m][n];
            current[n] = adds_i16(parent[n], s1 + s2);
        }
    }

    constexpr void update(Acc& current, const array<Fi, PieceList>& fi, int count) const {
        for (auto n : range<AccIndex>()) {
            _t a{}; // bias = 0
            for (int i = 0; i < count; ++i) {
                a = adds_i16(a, this->w0[ fi[PieceList{i}]][n] );
            }
            current[n] = a;
        }
    }

private:
    W0 w0; // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1; // output weights, 2*(64*32) = 4096 bytes
    B1 b1; // output bias (64 byte aligned), total = 1577024 bytes
};
extern const Nnue nnue;

#endif
