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

// NNUE feature layer index
struct Fi : ::Index<Fi, 6*2*64, i16_t> {
    constexpr Fi () : Index{} {}
    constexpr explicit Fi (_t v) : Index{v} {}
    constexpr Fi (Side side, Piece piece, Square sq) : Fi{ static_cast<_t>((+piece*128) + (+side*64) + (+sq)) } {}
    constexpr Fi (Square sqFlip) : Fi{ static_cast<_t>(+sqFlip) } {}

    constexpr Fi operator ^ (Fi mask) const { return Fi{ static_cast<_t>(v_ ^ mask.v_) }; }
    constexpr Fi operator ~ () const { return Fi{ static_cast<_t>(v_ ^ 64) }; } // change side
};

// (768x crelu -> 4x screlu) x256x2 -> 1 (2*256 channels each of 4 accumulator neurons)
struct CACHE_ALIGN Nnue {
    using _t = i16x16_t;
    static constexpr int Vector_lanes = sizeof(_t) / sizeof(i16_t);
    static constexpr int Channels = 256; static_assert(Channels % Vector_lanes == 0);
    static constexpr int Acc_neurons = 1024; static_assert(Acc_neurons % Channels == 0);

    struct ChannelIndex : Index<ChannelIndex, Channels / Vector_lanes> { using Index::Index; }; // 16
    struct StrideIndex : Index<StrideIndex, Acc_neurons / Channels> { using Index::Index; }; // 4

    using Acc = array<_t, ChannelIndex, StrideIndex>; // 1024/16 = 64
    using DualAcc = array<Acc, Side>; // 2*1024/16 = 128

    using W0 = array<_t, Fi, ChannelIndex, StrideIndex>; // QW0 = 2^10
    using W1 = DualAcc; // QW1 = 2^12
    using B1 = array<_t, Side, ChannelIndex>; // QB1 = 2^10
    using W2 = array<_t, Side, ChannelIndex>; // QW2 = 2^4 * WDL (WDL=400)
    using B2 = i32_t; // QB2 = QS2*QW2 = 2^15 * WDL

    i32_t evaluate(const DualAcc& dacc) const {
        i32x8_t sum15{};
        for (auto side : range<Side>()) {
            for (auto ch : range<ChannelIndex>()) {
                // 1 raw neuron value = 0.5 centipawns (when WDL=400)
                auto sum10{ this->b1[side][ch] }; // QB1 = 2^10
                for (auto n : range<StrideIndex>()) {
                    // accumulator SCReLU activation
                    auto x10 = dacc[side][ch][n]; // QW0 = 2^10
                    auto c14 = clamp(x10, 0, 1024) << 4; // 2^14
                    auto s13 = mulhrs_i16(c14, c14); // 2^13

                    // channel weighted sum
                    auto w12 = this->w1[side][ch][n]; // QW1 = 2^12
                    auto f10 = mulhrs_i16(s13, w12); // QB1 = 2^10
                    sum10 += f10; // 5 addends per stride
                }

                // channel SCReLU activation
                auto c13 = clamp(sum10, 0, 1024) << 3; // 2^13
                auto s11 = mulhrs_i16(c13, c13); // QS2 = 2^11

                // output weighted sum
                auto w4  = this->w2[side][ch]; // QW2 = 2^4 * WDL
                auto f15 = madd_i16(s11, w4); // QB2 = 2^15 * WDL
                sum15 += f15; // 64 addends
            }
        }
        auto result15 = this->b2 + hadd_i32(sum15); // QB2 = 2^15 * WDL
        auto result = result15 >> 15; // WDL
        return result;
    }

    constexpr void quiet(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1) const {
        for (auto ch : range<ChannelIndex>()) {
            for (auto n : range<StrideIndex>()) {
                current[ch][n] = adds_i16(parent[ch][n], this->w0[add1^m][ch][n] - this->w0[sub1^m][ch][n]);
            }
        }
    }

    constexpr void capture(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2) const {
        for (auto ch : range<ChannelIndex>()) {
            for (auto n : range<StrideIndex>()) {
                current[ch][n] = adds_i16(parent[ch][n], this->w0[add1^m][ch][n] - this->w0[sub1^m][ch][n] - this->w0[sub2^m][ch][n]);
            }
        }
    }

    constexpr void castling(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2, Fi add2) const {
        for (auto ch : range<ChannelIndex>()) {
            for (auto n : range<StrideIndex>()) {
                auto s1 = this->w0[add1^m][ch][n] - this->w0[sub1^m][ch][n];
                auto s2 = this->w0[add2^m][ch][n] - this->w0[sub2^m][ch][n];
                current[ch][n] = adds_i16(parent[ch][n], s1 + s2);
            }
        }
    }

    constexpr void update(Acc& current, const array<Fi, PieceList>& fi, int count) const {
        for (auto ch : range<ChannelIndex>()) {
            for (auto n : range<StrideIndex>()) {
                _t a{}; // bias = 0, feature biases embeded into kings weights
                for (int i = 0; i < count; ++i) {
                    a = adds_i16(a, this->w0[ fi[PieceList{i}]][ch][n] );
                }
                current[ch][n] = a;
            }
        }
    }

    COLD void validate_embedded() const;

private:
    // total 1579072 bytes
    W0 w0; // feature weights, 768*(2*1024) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1; // accumulator activation weights, 2*2048 = 4096 bytes
    B1 b1; // channels biases, 2*512 = 1024 bytes
    W2 w2; // channels output weights, 2*512 = 1024 bytes
    B2 b2; // output bias (64 byte aligned)
};
extern constinit const Nnue& nnue;

#endif
