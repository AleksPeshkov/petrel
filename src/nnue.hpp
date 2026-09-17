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
struct Fi : ::Index<Fi, 6*2*64, i16_t> { using Index::Index;
    constexpr Fi (Side side, Piece ty, Square sq)
        : Index{ static_cast<_t>((+ty*128) + (+side*64) + (+sq)) }
    {}
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

    W0 w0;    // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 2*(64*32) = 4096 bytes
    B1 b1; // output bias (64 byte aligned), total = 1577024 bytes

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
};
extern const Nnue nnue;

class Position;

class CACHE_ALIGN Acc {
    using Index = Nnue::AccIndex;
public:
    static constexpr void swap(Acc& my, Acc& op) {
        for (auto n : range<Index>()) {
            std::swap(my.acc[n], op.acc[n]);
        }
    }

    // defined in Position.cpp
    template <Side::_t>
    void setup(const Position& pos, Square sqFlip);

    constexpr void move(Square sqFlip, Side si, Piece ty, Square from, Square to) {
        move({si, ty, from^sqFlip}, {si, ty, to^sqFlip});
    }

    constexpr void promote(Square sqFlip, Side si, Square from, Officer promoted, Square to) {
        move({si, Pawn, from^sqFlip}, {si, promoted, to^sqFlip});
    }

    constexpr void move(Square sqFlip, Side si, Piece ty, Square from, Square to, NonKingPiece captured) {
        capture({si, ty, from^sqFlip}, {si, ty, to^sqFlip}, {~si, captured, to^sqFlip});
    }

    constexpr void promote(Square sqFlip, Side si, Square from, Officer promoted, Square to, NonKingPiece captured) {
        capture({si, Pawn, from^sqFlip}, {si, promoted, to^sqFlip}, {~si, captured, to^sqFlip});
    }

    constexpr void ep(Square sqFlip, Side si, Square from, Square to, Square ep) {
        capture({si, Pawn, from^sqFlip}, {si, Pawn, to^sqFlip}, {~si, Pawn, ep^sqFlip});
    }

    constexpr void castle(Square sqFlip, Side si, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        for (auto n : range<Index>()) {
            auto s1 = nnue.w0[{si, King, kingTo^sqFlip}][n] - nnue.w0[{si, King, kingFrom^sqFlip}][n];
            auto s2 = nnue.w0[{si, Rook, rookTo^sqFlip}][n] - nnue.w0[{si, Rook, rookFrom^sqFlip}][n];
            acc[n] = adds_i16(acc[n], s1 + s2);
        }
    }

private:
    Nnue::Acc acc; // feature biases embedded into kings weights

    constexpr void move(Fi from, Fi to) {
        for (auto n : range<Index>()) {
            acc[n] = adds_i16(acc[n], nnue.w0[to][n] - nnue.w0[from][n]);
        }
    }

    constexpr void capture(Fi from, Fi to, Fi cap) {
        for (auto n : range<Index>()) {
            acc[n] = adds_i16(acc[n], nnue.w0[to][n] - nnue.w0[from][n] - nnue.w0[cap][n]);
        }
    }
};

class DualAcc {
public:
    // raw NNUE static evaluation
    auto evaluate() const { return nnue.evaluate(std::bit_cast<Nnue::DualAcc>(dacc)); }

    // defined in Position.cpp
    void setup(const Position& pos);

    // copy parent accumulator but swap sides
    constexpr void copy_swap(const DualAcc& parent) {
        dacc[My] = parent.dacc[Op];
        dacc[Op] = parent.dacc[My];
        sqFlip[My] = parent.sqFlip[Op];
        sqFlip[Op] = parent.sqFlip[My];
    }

    constexpr void swap() {
        Acc::swap(dacc[My], dacc[Op]);
        std::swap(sqFlip[My], sqFlip[Op]);
    }

    constexpr void move(Piece ty, Square from, Square to) {
        assert (from != to);
        dacc[Op].move(sqFlip[Op], My, ty, from, to);
        dacc[My].move(~sqFlip[My], Op, ty, from, to);
    }

    constexpr void move(Piece ty, Square from, Square to, NonKingPiece captured) {
        assert (from != to);
        dacc[Op].move(sqFlip[Op], My, ty, from, to, captured);
        dacc[My].move(~sqFlip[My], Op, ty, from, to, captured);
    }

    constexpr void promote(Square from, Officer promoted, Square to) {
        assert (from.on(Rank7)); assert (to.on(Rank8));
        dacc[Op].promote(sqFlip[Op], My, from, promoted, to);
        dacc[My].promote(~sqFlip[My], Op, from, promoted, to);
    }

    constexpr void promote(Square from, Officer promoted, Square to, NonKingPiece captured) {
        assert (from.on(Rank7)); assert (to.on(Rank8));
        dacc[Op].promote(sqFlip[Op], My, from, promoted, to, captured);
        dacc[My].promote(~sqFlip[My], Op, from, promoted, to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.on(Rank5)); assert (to.on(Rank6)); assert (ep.on(Rank5));
        dacc[Op].ep(sqFlip[Op], My, from, to, ep);
        dacc[My].ep(~sqFlip[My], Op, from, to, ep);
    }

    // defined in Position.cpp
    constexpr void moveKing(const Position&, Square from, Square to);
    constexpr void moveKing(const Position&, Square from, Square to, NonKingPiece captured);
    constexpr void castle(const Position&, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo);

private:
    array<Acc, Side> dacc;
    array<Square, Side> sqFlip;
};

#endif
