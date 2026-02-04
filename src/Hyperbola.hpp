#ifndef HYPERBOLA_HPP
#define HYPERBOLA_HPP

#include "bitops128.hpp"
#include "Bb.hpp"

// bitreverse all 128-bits (actually only 64-bits are needed)
constexpr vu8x16_t bitSwap(vu8x16_t v) {
    constexpr vu8x16_t nibbleSwap{
        0b0000, 0b1000, 0b0100, 0b1100,  // 0000 → 0000, 0001 → 1000, 0010 → 0100, 0011 → 1100
        0b0010, 0b1010, 0b0110, 0b1110,  // 0100 → 0010, 0101 → 1010, 0110 → 0110, 0111 → 1110
        0b0001, 0b1001, 0b0101, 0b1101,  // 1000 → 0001, 1001 → 1001, 1010 → 0101, 1011 → 1101
        0b0011, 0b1011, 0b0111, 0b1111,  // 1100 → 0011, 1101 → 1011, 1110 → 0111, 1111 → 1111
    };
    auto hi = shufflevector(nibbleSwap, (v >> 4) & 0b1111);
    auto lo = shufflevector(nibbleSwap << 4, v & 0b1111);
    return lo | hi;
}

constexpr vu8x16_t byteSwap(vu8x16_t v) {
    constexpr vu8x16_t _byteSwap{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,};
    return shufflevector(v, _byteSwap);
}

constexpr vu64x2_t bitReverse(vu64x2_t v) {
    #if defined(__aarch64__) || defined(_M_ARM64)
        // Fast path: ARM64 rbit
        alignas(16) uint64_t parts[2];
        memcpy(parts, &v, sizeof(parts));
        return vu64x2_t{ __rbit64(parts[1]), __rbit64(parts[0]) };
    #else
        return byteSwap(bitSwap(v));
    #endif
}

//TRICK: Square operator~ is different
constexpr Square reverse(Square sq) { return Square{ ~File{sq}, ~Rank{sq} }; }

struct CACHE_ALIGN HyperbolaSq : Square::arrayOf<vu64x2_t> {
    constexpr HyperbolaSq () {
        for (auto sq :  range<Square>()) {
            (*this)[sq] = vu64x2_t{ Bb{sq}, Bb{::reverse(sq)} };
        }
    }
};
extern const HyperbolaSq hyperbolaSq;

struct CACHE_ALIGN HyperbolaDir : Square::arrayOf<Direction::arrayOf<vu64x2_t>> {
    constexpr HyperbolaDir () {
        for (auto sq :  range<Square>()) {
            Square rsq{::reverse(sq)};
            for (auto dir : range<Direction>()) {
                (*this)[sq][dir] = vu64x2_t{sq.line(dir), rsq.line(dir)};
            }
        }
    }
};
extern const HyperbolaDir hyperbolaDir;

/**
 * Vector of bitboard and its bitreversed complement
 * used for sliding pieces attacks generation
 * https://www.chessprogramming.org/Hyperbola_Quintessence
 */
class alignas(32) Hyperbola {
    vu64x2_t occupied;

    constexpr static vu64x2_t hyperbola(vu64x2_t v) { return v ^ bitReverse(v); }

public:
    // hyperbola({bb1, 0}) == {bb ^ bitreverse64(0), 0 ^ bitreverse64(bb)} == {bb, bitreverse64(bb)}
    explicit Hyperbola (Bb bb) : occupied{ hyperbola(vu64x2_t{bb, 0}) } {}

    constexpr Bb attack(SliderType ty, Square from) const {
        const auto& sq = hyperbolaSq[from];

        // branchless computation
        Direction dir{ ty == Bishop ? DiagonalDir : FileDir };

        const auto& d0 = hyperbolaDir[from][dir];
        const auto& d1 = hyperbolaDir[from][Direction{static_cast<Direction::_t>(dir+1)}];

        // bishop attacks for Bishops, rooks attacks for Rooks and Queens
        auto result = ((occupied & d0) - sq) & d0;
        result     |= ((occupied & d1) - sq) & d1;

        if (ty == Queen) {
            // plus bishop attacks for Queens
            const auto& d = hyperbolaDir[from][Direction{DiagonalDir}];
            const auto& a = hyperbolaDir[from][Direction{AntidiagDir}];
            result |= ((occupied & d) - sq) & d;
            result |= ((occupied & a) - sq) & a;
        }

        // hyperbola({bb1, bb2}) == { bb1 ^ bitreverse64(bb2), bb2 ^ bitreverse64(bb1) }
        result = hyperbola(result);

        return Bb{::u64(result)};
    }
};

#endif
