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

    enum unit_enum { UnitX, UnitY };
    struct UnitIndex : Index<UnitIndex, 2, unit_enum> { constexpr UnitIndex (_t i) : Index{i} {} };

    struct AccIndex : Index<AccIndex, (Acc_neurons / lanes_of<_t>) / UnitIndex::size()> { using Index::Index; }; // 32
    using Acc = array<_t, AccIndex, UnitIndex>;
    using DualAcc = array<Acc, Side>;

    using W0 = array<_t, Fi, AccIndex, UnitIndex>; // QW0 = 2^10
    using W1 = array<_t, Side, AccIndex, UnitIndex>; // QW1 = 2^5 * WDL

    i32_t evaluate(const DualAcc& dacc) const {
        i32x8_t sum10{};
        for (auto side : range<Side>()) {
            for (auto n : range<AccIndex>()) {
                auto x10 = dacc[side][n][UnitX]; // x: QW0 = 2^10
                auto y10 = dacc[side][n][UnitY]; // y: QW0 = 2^10

                auto a10 = max(x10, y10);
                auto wa5 = w1[side][n][UnitX]; // Wa = Wmax: QW0 = 2^10
                auto sa0 = mulhrs_i16(wa5, a10); // A * a : WDL

                // A*max(rx,ry)^2 + B*rx*ry = max(rx,ry)*(A*max(x,y) + B*min(rx,ry))
                auto b10 = min(x10, y10);
                auto rb10 = relu(b10);
                auto wb5 = w1[side][n][UnitY]; // Wb = Wmin: QW0 = 2^10
                auto sb0 = mulhrs_i16(wb5, rb10); // B * rb : WDL

                auto ra10 = relu(a10);
                auto s0  = adds_i16(sa0, sb0); // A*a + B*rb : WDL
                auto f10 = madd_i16(ra10, s0); // ra * (A*a + B*rb): 2^10 * WDL
                sum10 += f10; // 32 addends per side
            }
        }
        auto output10 = hadd(sum10); // 2^10 * WDL, bias = 0
        auto output = output10 >> 10; // WDL
        return output;
    }

    constexpr void quiet(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1) const {
        for (auto n : range<AccIndex>()) {
            for (auto u : range<UnitIndex>()) {
                current[n][u] = adds_i16(parent[n][u], this->w0[add1^m][n][u] - this->w0[sub1^m][n][u]);
            }
        }
    }

    constexpr void capture(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2) const {
        for (auto n : range<AccIndex>()) {
            for (auto u : range<UnitIndex>()) {
                current[n][u] = adds_i16(parent[n][u], this->w0[add1^m][n][u] - this->w0[sub1^m][n][u] - this->w0[sub2^m][n][u]);
            }
        }
    }

    constexpr void castling(Acc& current, const Acc& parent, Fi m, Fi sub1, Fi add1, Fi sub2, Fi add2) const {
        for (auto n : range<AccIndex>()) {
            for (auto u : range<UnitIndex>()) {
                auto s1 = this->w0[add1^m][n][u] - this->w0[sub1^m][n][u];
                auto s2 = this->w0[add2^m][n][u] - this->w0[sub2^m][n][u];
                current[n][u] = adds_i16(parent[n][u], s1 + s2);
            }
        }
    }

    constexpr void update(Acc& current, const array<Fi, PieceList>& fi, int count) const {
        for (auto n : range<AccIndex>()) {
            for (auto u : range<UnitIndex>()) {
                _t a{}; // bias = 0, feature biases embeded into kings weights
                for (int i = 0; i < count; ++i) {
                    a = adds_i16(a, this->w0[ fi[{i}] ][n][u] );
                }
                current[n][u] = a;
            }
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
