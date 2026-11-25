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

    enum unit_enum { SignalX, SignalY };
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
                auto x10 = dacc[side][n][SignalX]; // x: QW0 = 2^10
                auto wx5 = this->w1[side][n][SignalX]; // Wx: QW1 = 2^5 * WDL
                auto sx0 = mulhrs_i16(wx5, x10); // Sx = Wx * x : WDL

                auto y10 = dacc[side][n][SignalY]; // y: QW0 = 2^10
                auto wy5 = this->w1[side][n][SignalY]; // Wy: QW1 = 2^5 * WDL
                auto sy0 = mulhrs_i16(wy5, y10); // Sy = Wy * y : WDL

                // bipolar gate: relu(x) - relu(y)
                auto rx10 = relu(x10);
                auto ry10 = relu(y10);
                auto gr10 = rx10 - ry10;
                auto gs0 = gr10 > 0 ? sx0 : sy0; // gated signal: only one (or none) can be active at a time

                // Concatenated ReLU trick: relu(z) + relu(-z)
                // Sx * relu( relu(x)-relu(y) ) + Sy * -relu( -(relu(x)-relu(y)) )
                auto a10 = madd_i16(gr10, gs0); // complete activation: 2^10 * WDL
                sum10 += a10; // 32 addends per side
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
