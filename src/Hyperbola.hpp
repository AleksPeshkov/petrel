#ifndef HYPERBOLA_HPP
#define HYPERBOLA_HPP

#include "BitArray128.hpp"
#include "BitReverse.hpp"
#include "Bb.hpp"

typedef i128_t u64x2_t;

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
