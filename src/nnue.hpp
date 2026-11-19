#ifndef NNUE_HPP
#define NNUE_HPP

#include "simd256.hpp"
#include "Index.hpp"

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
    static constexpr int Acc_neurons = 1024;

    struct AccIndex : Index<AccIndex, Acc_neurons / lanes_of<_t>> { using Index::Index; }; // 64
    using Acc = array<_t, AccIndex>;
    using DualAcc = array<Acc, Side>;

    using W0 = array<_t, Fi, AccIndex>; // QW0 = 2^10
    using W1 = DualAcc; // QW1 = 2^5 * WDL (WDL = 400)

    i32_t evaluate(const DualAcc& dacc) const {
        i32x8_t sum10{};
        for (auto side : range<Side>()) {
            for (auto n : range<AccIndex>()) {
                // W * x
                auto x10 = dacc[side][n]; // x: QW0 = 2^10
                auto w5  = this->w1[side][n]; // W: QW1 = 2^5 * WDL
                auto s0  = mulhrs_i16(x10, w5); // signal (weighted input): WDL
                auto r10 = relu(x10); // gate: 2^10

                // W * x * relu(x) = W * relu(x)^2
                auto a10 = madd_i16(s0, r10); // activation: 2^10 * WDL
                sum10 += a10; // 2^10 * WDL
            }
        }
        auto output10 = hadd(sum10); // 2^10 * WDL, bias = 0
        auto output = output10 >> 10; // WDL
        return output;
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
            _t a{}; // bias = 0, feature biases embeded into kings weights
            for (int i = 0; i < count; ++i) {
                a = adds_i16(a, this->w0[ fi[{i}] ][n] );
            }
            current[n] = a;
        }
    }

    COLD void validate_embedded() const;

private:
    // total 1576960 bytes
    W0 w0; // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1; // output weights, 2*(64*32) = 4096 bytes
};
extern constinit const Nnue& nnue;

#endif
