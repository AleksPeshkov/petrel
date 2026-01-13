#ifndef PI_BB_H
#define PI_BB_H

#include "Bb.hpp"
#include "PiMask.hpp"

/// array of 8 PiRank
class PiBbMatrix {
    Rank::arrayOf<PiRank> matrix;

public:
    constexpr PiBbMatrix () = default;

    constexpr friend void swap(PiBbMatrix& a, PiBbMatrix& b) {
        for (auto rank : Rank::range()) {
            using std::swap;
            swap(a.matrix[rank], b.matrix[rank]);
        }
    }

    constexpr void clear() {
        for (auto rank : Rank::range()) {
            matrix[rank].clear();
        }
    }

    constexpr void clear(Pi pi, Square sq) {
        matrix[Rank{sq}] -= PiRank{File{sq}} & PiMask{pi};
    }

    constexpr void clear(Pi pi) {
        for (auto rank : Rank::range()) {
            matrix[rank] %= PiMask{pi};
        }
    }

    constexpr void set(Pi pi, Rank::_t rank, BitRank br) {
        //_mm_blendv_epi8
        matrix[rank] = (matrix[rank] % PiMask{pi}) + (PiRank{br} & PiMask{pi});
    }

    constexpr void set(Pi pi, Bb bb) {
        for (auto rank : Rank::range()) {
            set(pi, rank, bb[rank]);
        }
    }

    constexpr void add(Pi pi, Rank::_t rank, File file) {
        matrix[rank] += PiRank{file} & PiMask{pi};
    }

    constexpr void add(Pi pi, Square sq) {
        add(pi, Rank{sq}, File{sq});
    }

    constexpr bool has(Pi pi, Square sq) const {
        return (matrix[Rank{sq}] & PiRank{File{sq}} & PiMask{pi}).any();
    }

    constexpr const PiRank& operator[] (Rank::_t rank) const {
        return matrix[rank];
    }

    constexpr PiRank& operator[] (Rank::_t rank) {
        return matrix[rank];
    }

    //pieces affecting the given square
    PiMask operator[] (Square sq) const {
        return matrix[Rank{sq}][File{sq}];
    }

    //bitboard of the given piece
    constexpr Bb operator[] (Pi pi) const {
        union {
            Bb::_t bb;
            Rank::arrayOf<BitRank::_t> br;
        };
        for (auto rank : Rank::range()) {
            br[rank] = matrix[rank][pi];
        }
        return Bb{bb};
    }

    //bitboard of squares affected by all pieces
    constexpr Bb gather() const {
        union {
            Bb::_t bb;
            Rank::arrayOf<BitRank::_t> br;
        };
        for (auto rank : Rank::range()) {
            br[rank] = matrix[rank].gather();
        }
        return Bb{bb};
    }

    void filter(Pi pi, Bb bb) {
        PiMask exceptPi{ PiMask::all() ^ PiMask{pi} };
        for (auto rank : Rank::range()) {
            matrix[rank] &= PiRank{bb[rank]} | exceptPi;
        }
    }

    constexpr friend PiBbMatrix operator % (const PiBbMatrix& from, Bb bb) {
        PiBbMatrix result;
        for (auto rank : Rank::range()) {
            result.matrix[rank] = from.matrix[rank] % PiRank{bb[rank]};
        }
        return result;
    }

    constexpr void operator &= (Bb bb) {
        for (auto rank : Rank::range()) {
            matrix[rank] &= PiRank{bb[rank]};
        }
    }

    constexpr friend PiBbMatrix operator & (const PiBbMatrix& from, Bb bb) {
        PiBbMatrix result;
        for (auto rank : Rank::range()) {
            result.matrix[rank] = from.matrix[rank] & PiRank{bb[rank]};
        }
        return result;
    }

    constexpr bool none() const {
        for (auto rank : Rank::range()) {
            if (matrix[rank].any()) { return false; }
        }
        return true;
    }

    constexpr int popcount() const {
        int sum = 0;
        for (auto rank : Rank::range()) {
            sum += ::popcount(matrix[rank]);
        }
        return sum;
    }
};

#endif
