#ifndef NNUE_HPP
#define NNUE_HPP

#include "simd256.hpp"
#include "Index.hpp"

inline i64x4_t unpack_add_i32(i32x8_t a) {
    // signed extension from i32 to i64
    i64x4_t low = __builtin_convertvector(__builtin_shufflevector(a, a, 0, 1, 2, 3), i64x4_t);
    i64x4_t high = __builtin_convertvector(__builtin_shufflevector(a, a, 4, 5, 6, 7), i64x4_t);
    return low + high;
}

struct CACHE_ALIGN Nnue {
    struct FeatureIndex : ::Index<FeatureIndex, 2*6*64> { using Index::Index;
        constexpr FeatureIndex (Side side, Piece piece, Square sq)
            : Index{ (+piece*128) + (+side*64) + (+sq) }
        {}
    };

    using _t = i16x16_t;
    static constexpr int Acc_neurons = 1024;

    struct AccIndex : Index<AccIndex, Acc_neurons / lanes_of<_t>> { using Index::Index; };
    struct DualAccIndex : Index<DualAccIndex, 2*AccIndex::size()> { using Index::Index; };

    using W0 = array<_t, FeatureIndex, AccIndex>;
    using W1 = array<_t, DualAccIndex>;

    W0 w0;    // feature weights, 768*(64*32) = 1572864 bytes, feature biases embeded into kings weights
    W1 w1;    // output weights, 2*(64*32) = 4096 bytes
    i32_t b1; // output bias (64 byte aligned), total = 1577024 bytes

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
        i64_t output = this->b1 + hadd(sum4);

        constexpr auto Scale = 14; // QA*QA: 2*10, QB: 5, shift: 4, mulhrs_i16: -15
        auto result = output >> Scale;
        return result;
    }
};
extern const Nnue nnue;

class Position;

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

    // defined in Position.cpp
    template <Side::_t>
    void setup(const Position& pos, Square mirror);

    constexpr void move(Square mirror, Side side, Piece piece, Square from, Square to) {
        move({side, piece, from^mirror}, {side, piece, to^mirror});
    }

    constexpr void promote(Square mirror, Side side, Square from, Officer promoted, Square to) {
        move({side, Pawn, from^mirror}, {side, promoted, to^mirror});
    }

    constexpr void move(Square mirror, Side side, Piece piece, Square from, Square to, NonKingPiece captured) {
        capture({side, piece, from^mirror}, {side, piece, to^mirror}, {~side, captured, to^mirror});
    }

    constexpr void promote(Square mirror, Side side, Square from, Officer promoted, Square to, NonKingPiece captured) {
        capture({side, Pawn, from^mirror}, {side, promoted, to^mirror}, {~side, captured, to^mirror});
    }

    constexpr void ep(Square mirror, Side side, Square from, Square to, Square ep) {
        capture({side, Pawn, from^mirror}, {side, Pawn, to^mirror}, {~side, Pawn, ep^mirror});
    }

    constexpr void castle(Square mirror, Side side, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        for (auto n : range<AccIndex>()) {
            auto s1 = nnue.w0[{side, King, kingTo^mirror}][n] - nnue.w0[{side, King, kingFrom^mirror}][n];
            auto s2 = nnue.w0[{side, Rook, rookTo^mirror}][n] - nnue.w0[{side, Rook, rookFrom^mirror}][n];
            acc[n] = adds_i16(acc[n], s1 + s2);
        }
    }

private:
    array<_t, AccIndex> acc{}; // feature biases embedded into kings weights

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

    constexpr void move(Piece piece, Square from, Square to) {
        assert (from != to);
        side[Op].move(mirror[Op], My, piece, from, to);
        side[My].move(~mirror[My], Op, piece, from, to);
    }

    constexpr void move(Piece piece, Square from, Square to, NonKingPiece captured) {
        assert (from != to);
        side[Op].move(mirror[Op], My, piece, from, to, captured);
        side[My].move(~mirror[My], Op, piece, from, to, captured);
    }

    constexpr void promote(Square from, Officer promoted, Square to) {
        assert (from.isOn(Rank7)); assert (to.isOn(Rank8));
        side[Op].promote(mirror[Op], My, from, promoted, to);
        side[My].promote(~mirror[My], Op, from, promoted, to);
    }

    constexpr void promote(Square from, Officer promoted, Square to, NonKingPiece captured) {
        assert (from.isOn(Rank7)); assert (to.isOn(Rank8));
        side[Op].promote(mirror[Op], My, from, promoted, to, captured);
        side[My].promote(~mirror[My], Op, from, promoted, to, captured);
    }

    constexpr void ep(Square from, Square to, Square ep) {
        assert (from.isOn(Rank5)); assert (to.isOn(Rank6)); assert (ep.isOn(Rank5));
        side[Op].ep(mirror[Op], My, from, to, ep);
        side[My].ep(~mirror[My], Op, from, to, ep);
    }

    // defined in Position.cpp
    constexpr void moveKing(const Position&, Square from, Square to);
    constexpr void moveKing(const Position&, Square from, Square to, NonKingPiece captured);
    constexpr void castle(const Position&, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo);

private:
    array<Acc, Side> side{};
    array<Square, Side> mirror{};
};

#endif
