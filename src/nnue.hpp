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
        return a < b ? b : a;
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

constexpr i16x16_t relu(i16x16_t a) {
    return max(a, i16x16x(0));
}

constexpr i16x16_t clamp(i16x16_t a, int low, int high) {
    return min(max(a, i16x16x(low)), i16x16x(high));
}

// _mm256_mulhrs_epi16
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

// _mm256_madd_epi16
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

inline i32_t hadd_i32(i32x8_t sum8) {
    #ifdef __clang__
        return __builtin_reduce_add(sum8);
    #else
        return sum8[0] + sum8[1] + sum8[2] + sum8[3] + sum8[4] + sum8[5] + sum8[6] + sum8[7];
    #endif
}

inline i32_t hadd_i16(i16x16_t sum16) {
    #ifdef __clang__
        return __builtin_reduce_add(sum16);
    #else
        return sum8[0] + sum8[1] + sum8[2] + sum8[3] + sum8[4] + sum8[5] + sum8[6] + sum8[7]
            + sum8[8] + sum8[9] + sum8[10] + sum8[11] + sum8[12] + sum8[13] + sum8[14] + sum8[15];
    #endif
}

// NNUE feature layer index
struct Fi : ::Index<Fi, 6*2*64, i16_t> {
    constexpr Fi () : Index{} {}
    constexpr explicit Fi (_t v) : Index{v} {}
    constexpr Fi (Side side, Piece piece, Square sq) : Fi{ static_cast<_t>((+piece*128) + (+side*64) + (+sq)) } {}
    constexpr Fi (Square sqFlip) : Fi{ static_cast<_t>(+sqFlip) } {}

    constexpr Fi operator ^ (Fi mask) const { return Fi{ static_cast<_t>(v_ ^ mask.v_) }; }
    constexpr Fi operator ~ () const { return Fi{ static_cast<_t>(v_ ^ 64) }; } // change side
};

struct CACHE_ALIGN Nnue {
    using _t = i16x16_t;
    static constexpr int Vector_size = sizeof(_t) / sizeof(i16_t);
    static constexpr int Acc_neurons = 1024;

    enum pair_enum { Pos, Neg };
    struct PairIndex : Index<PairIndex, 2, pair_enum> { constexpr PairIndex (_t i) : Index{i} {} };

    struct AccIndex : Index<AccIndex, (Acc_neurons / Vector_size) / PairIndex::size()> { using Index::Index; }; // 32
    using Acc = array<_t, AccIndex, PairIndex>;
    using DualAcc = array<Acc, Side>;

    using W0 = array<_t, Fi, AccIndex, PairIndex>; // QW0 = 2^10
    using W1 = array<_t, Side, AccIndex>; // QW1 = 2^6 * WDL
    using B1 = i32_t; // QB1 = WDL (WDL = ~400)

    i32_t evaluate(const DualAcc& dacc) const {
        i16x16_t sum{};
        for (auto side : range<Side>()) {
            for (auto n : range<AccIndex>()) {
                // accumulator ReLU(ReLU(x_p) - ReLU(x_n)) activation
                auto p10 = relu(dacc[side][n][Pos]); // QW0 = 2^10
                auto n10 = relu(dacc[side][n][Neg]); // QW0 = 2^10
                auto d10 = relu(p10 - n10);

                // saturated Squared ReLU
                auto d11 = adds_i16(d10, d10);
                auto d12 = adds_i16(d11, d11);
                auto m9  = mulhrs_i16(d12, d12); // 2^9

                // output weighted sum
                auto w6 = this->w1[side][n]; // QW1 = 2^6 * WDL
                auto f = mulhrs_i16(m9, w6); // WDL
                sum = adds_i16(sum, f); // 32 addends per side
            }
        }
        auto output = this->b1 + hadd_i16(sum); // WDL
        return output;
    }

    constexpr void quiet(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1) const {
        for (auto n : range<AccIndex>()) {
            for (auto p : range<PairIndex>()) {
                current[n][p] = adds_i16(parent[n][p], this->w0[add1^m][n][p] - this->w0[sub1^m][n][p]);
            }
        }
    }

    constexpr void capture(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2) const {
        for (auto n : range<AccIndex>()) {
            for (auto p : range<PairIndex>()) {
                current[n][p] = adds_i16(parent[n][p], this->w0[add1^m][n][p] - this->w0[sub1^m][n][p] - this->w0[sub2^m][n][p]);
            }
        }
    }

    constexpr void castling(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2, Fi add2) const {
        for (auto n : range<AccIndex>()) {
            for (auto p : range<PairIndex>()) {
                auto s1 = this->w0[add1^m][n][p] - this->w0[sub1^m][n][p];
                auto s2 = this->w0[add2^m][n][p] - this->w0[sub2^m][n][p];
                current[n][p] = adds_i16(parent[n][p], s1 + s2);
            }
        }
    }

    constexpr void update(Acc& current, const array<Fi, PieceList>& fi, int count) const {
        for (auto n : range<AccIndex>()) {
            for (auto p : range<PairIndex>()) {
                _t a{}; // bias = 0, feature biases embeded into kings weights
                for (int i = 0; i < count; ++i) {
                    a = adds_i16(a, this->w0[ fi[{i}]][n][p] );
                }
                current[n][p] = a;
            }
        }
    }

    COLD void validate_embedded() const;

private:
    // total 1574976 bytes
    W0 w0; // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1; // output weights, 2*(32*32) = 2048 bytes
    B1 b1; // output bias (64 byte aligned)
};
extern constinit const Nnue& nnue;

#endif
