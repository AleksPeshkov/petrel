#ifndef HYPERBOLA_HPP
#define HYPERBOLA_HPP

#include "BitArray128.hpp"
#include "Bb.hpp"
#include "VectorOfAll.hpp"

typedef i128_t u64x2_t;

class BitReverse {
    const u8x16_t nibbleSwap = {{0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e, 0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f,}};
    const u8x16_t _byteSwap = {{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,}};

public:
    i128_t bitSwap(i128_t v) const {
        constexpr auto& nibbleMask = ::vectorOfAll[0x0f];
        auto hi = _mm_shuffle_epi8(nibbleSwap, _mm_and_si128(nibbleMask, _mm_srli_epi16(v, 4)));
        auto lo = _mm_shuffle_epi8(_mm_slli_epi16(nibbleSwap, 4), _mm_and_si128(nibbleMask, v));
        return hi | lo;
    }

    i128_t byteSwap(i128_t v) const {
        return _mm_shuffle_epi8(v, _byteSwap);
    }

    i128_t operator() (i128_t v) const {
        return byteSwap(bitSwap(v));
    }

};

extern const BitReverse bitReverse;

//TRICK: Square operator~ is different
constexpr Square reverse(Square sq) { return { ~File{sq}, ~Rank{sq} }; }

struct HyperbolaSq : Square::arrayOf<u64x2_t> {
    HyperbolaSq () {
        FOR_EACH(Square, sq) {
            (*this)[sq] = u64x2_t{ Bb{sq}, Bb{::reverse(sq)} };
        }
    }
};
extern const HyperbolaSq hyperbolaSq;

struct alignas(64) HyperbolaDir : Square::arrayOf<Direction::arrayOf<u64x2_t>> {
    HyperbolaDir () {
        FOR_EACH(Square, sq) {
            Square rsq = reverse(sq);
            FOR_EACH(Direction, dir) {
                (*this)[sq][dir] = u64x2_t{sq.line(dir), rsq.line(dir)};
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
class Hyperbola : public BitArray<Hyperbola, u64x2_t> {
    typedef u64x2_t _t;
    _t occupied;

    static _t hyperbola(_t v) { return v ^ ::bitReverse(v); }

public:
    explicit Hyperbola (Bb bb) : occupied{ hyperbola(u64x2_t{bb, 0}) } {}

    Bb attack(SliderType ty, Square from) const {
        const auto& sq = hyperbolaSq[from];

        // branchless computation
        Direction dir = (ty == Bishop) ? DiagonalDir : FileDir;

        const auto& d0 = hyperbolaDir[from][dir];
        const auto& d1 = hyperbolaDir[from][static_cast<direction_t>(dir+1)];

        // bishop attacks for Bishops, rooks attacks for Rooks and Queens
        auto result = d0 & _mm_sub_epi64(occupied & d0, sq);
        result |= d1 & _mm_sub_epi64(occupied & d1, sq);

        if (ty == Queen) {
            // plus bishop attacks for Queens
            const auto& d = hyperbolaDir[from][DiagonalDir];
            const auto& a = hyperbolaDir[from][AntidiagDir];
            result |= d & _mm_sub_epi64(occupied & d, sq);
            result |= a & _mm_sub_epi64(occupied & a, sq);
        }

        return Bb{ _mm_cvtsi128_si64(hyperbola(result)) };
    }

};

#endif
