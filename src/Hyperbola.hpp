#ifndef HYPERBOLA_HPP
#define HYPERBOLA_HPP

#include "BitArray128.hpp"
#include "Bb.hpp"
#include "VectorOfAll.hpp"

class BitReverse {
public:
    constexpr vu8x16_t bitSwap(vu8x16_t v) const {
        constexpr vu8x16_t nibbleSwap{0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e, 0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f,};
        constexpr auto& nibbleMask = ::vectorOfAll[0x0f].vu8x16;
        auto hi = shufflevector(nibbleSwap, nibbleMask & (v >> 4));
        auto lo = shufflevector(nibbleSwap << 4, nibbleMask & v);
        return hi | lo;
    }

    constexpr vu8x16_t byteSwap(vu8x16_t v) const {
        constexpr vu8x16_t _byteSwap{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,};
        return shufflevector(v, _byteSwap);
    }

    constexpr vu64x2_t operator() (vu64x2_t v) const {
        return byteSwap(bitSwap(v));
    }

};

extern const BitReverse bitReverse;

//TRICK: Square operator~ is different
constexpr Square reverse(Square sq) { return { ~File{sq}, ~Rank{sq} }; }

struct HyperbolaSq : Square::arrayOf<vu64x2_t> {
    HyperbolaSq () {
        FOR_EACH(Square, sq) {
            (*this)[sq] = vu64x2_t{ Bb{sq}, Bb{::reverse(sq)} };
        }
    }
};
extern const HyperbolaSq hyperbolaSq;

struct alignas(64) HyperbolaDir : Square::arrayOf<Direction::arrayOf<vu64x2_t>> {
    HyperbolaDir () {
        FOR_EACH(Square, sq) {
            Square rsq = reverse(sq);
            FOR_EACH(Direction, dir) {
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
class Hyperbola {
    vu64x2_t occupied;

    constexpr static vu64x2_t hyperbola(vu64x2_t v) { return v ^ ::bitReverse(v); }

public:
    explicit Hyperbola (Bb bb) : occupied{ hyperbola(vu64x2_t{bb, 0}) } {}

    constexpr Bb attack(SliderType ty, Square from) const {
        const auto& sq = hyperbolaSq[from];

        // branchless computation
        Direction dir = (ty == Bishop) ? DiagonalDir : FileDir;

        const auto& d0 = hyperbolaDir[from][dir];
        const auto& d1 = hyperbolaDir[from][static_cast<direction_t>(dir+1)];

        // bishop attacks for Bishops, rooks attacks for Rooks and Queens
        auto result = ((occupied & d0) - sq) & d0;
        result     |= ((occupied & d1) - sq) & d1;

        if (ty == Queen) {
            // plus bishop attacks for Queens
            const auto& d = hyperbolaDir[from][DiagonalDir];
            const auto& a = hyperbolaDir[from][AntidiagDir];
            result |= ((occupied & d) - sq) & d;
            result |= ((occupied & a) - sq) & a;
        }

        union { vu64x2_t u64x2; u64_t u64[2]; } bb = { hyperbola(result) };

        return Bb{ bb.u64[0] };
    }
};

#endif
