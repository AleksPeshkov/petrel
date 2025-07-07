#ifndef ATTACK_BB_HPP
#define ATTACK_BB_HPP

#include "BitArray128.hpp"
#include "BitReverse.hpp"
#include "Bb.hpp"

typedef i128_t u64x2_t;

constexpr u64x2_t combine(Bb lo, Bb hi) {
    return u64x2_t{
        static_cast<long long>(static_cast<Bb::_t>(lo)),
        static_cast<long long>(static_cast<Bb::_t>(hi))
    };
}

//TRICK: Square operator~ is different
constexpr Square reverse(Square sq) { return { ~File{sq}, ~Rank{sq} }; }

struct HyperbolaSq : Square::arrayOf<u64x2_t> {
    HyperbolaSq () {
        FOR_EACH(Square, sq) {
            (*this)[sq] = ::combine(Bb{sq}, Bb{::reverse(sq)});
        }
    }
};
extern const HyperbolaSq hyperbolaSq;

enum direction_t { Horizont, Vertical, Diagonal, Antidiag };
typedef Index<4, direction_t>::arrayOf<u64x2_t> Directions;
struct alignas(64) HyperbolaDir : SliderType::arrayOf< Square::arrayOf<Directions> > {
    HyperbolaDir () {
        constexpr u64x2_t empty = {0, 0};
        FOR_EACH(Square, sq) {
            Square rsq = ::reverse(sq);

            (*this)[Queen][sq][Horizont] = ::combine(sq.horizont(), rsq.horizont());
            (*this)[Queen][sq][Vertical] = ::combine(sq.vertical(), rsq.vertical());
            (*this)[Queen][sq][Diagonal] = ::combine(sq.diagonal(), rsq.diagonal());
            (*this)[Queen][sq][Antidiag] = ::combine(sq.antidiag(), rsq.antidiag());

            (*this)[Rook][sq][Horizont] = ::combine(sq.horizont(), rsq.horizont());
            (*this)[Rook][sq][Vertical] = ::combine(sq.vertical(), rsq.vertical());
            (*this)[Rook][sq][Diagonal] = empty;
            (*this)[Rook][sq][Antidiag] = empty;

            (*this)[Bishop][sq][Horizont] = empty;
            (*this)[Bishop][sq][Vertical] = empty;
            (*this)[Bishop][sq][Diagonal] = ::combine(sq.diagonal(), rsq.diagonal());
            (*this)[Bishop][sq][Antidiag] = ::combine(sq.antidiag(), rsq.antidiag());
        }
    }
};
extern const HyperbolaDir hyperbolaDir;

/**
 * Vector of bitboard and its bitreversed complement
 * used for sliding pieces attacks generation
 */
class AttackBb : public BitArray<AttackBb, u64x2_t> {
    typedef u64x2_t _t;
    _t occupied;

    static _t hyperbola(_t v) { return v ^ ::bitReverse(v); }

public:
    explicit AttackBb (Bb bb) : occupied{ hyperbola(::combine(bb, Bb{})) } {}

    Bb attack(SliderType type, Square from) const {
        const auto& dir = ::hyperbolaDir[type][from];
        const auto& sq = ::hyperbolaSq[from];

        auto h = dir[Horizont] & _mm_sub_epi64(occupied & dir[Horizont], sq);
        auto e = dir[Vertical] & _mm_sub_epi64(occupied & dir[Vertical], sq);
        auto d = dir[Diagonal] & _mm_sub_epi64(occupied & dir[Diagonal], sq);
        auto a = dir[Antidiag] & _mm_sub_epi64(occupied & dir[Antidiag], sq);

        return Bb{ static_cast<Bb::_t>( _mm_cvtsi128_si64(hyperbola(h | e | d | a)) ) };
    }

};

#endif
