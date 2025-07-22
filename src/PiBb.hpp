#ifndef PI_BB_H
#define PI_BB_H

#include "Bb.hpp"
#include "PiRank.hpp"

/// array of 8 PiRank
class PiBb {
    Rank::arrayOf<PiRank> matrix;

public:
    constexpr PiBb () = default;

    friend void swap(PiBb& a, PiBb& b) {
        FOR_EACH(Rank, rank) {
            using std::swap;
            swap(a.matrix[rank], b.matrix[rank]);
        }
    }

    void clear() {
        FOR_EACH(Rank, rank) {
            matrix[rank].clear();
        }
    }

    void clear(Pi pi, Square sq) {
        matrix[Rank(sq)] -= PiRank{File(sq)} & PiMask{pi};
    }

    void clear(Pi pi) {
        FOR_EACH(Rank, rank) {
            matrix[rank] %= PiMask{pi};
        }
    }

    void set(Pi pi, Rank rank, BitRank br) {
        //_mm_blendv_epi8
        matrix[rank] = (matrix[rank] % PiMask{pi}) + (PiRank{br} & PiMask{pi});
    }

    void set(Pi pi, Bb bb) {
        FOR_EACH(Rank, rank) {
            set(pi, rank, bb[rank]);
        }
    }

    void add(Pi pi, Rank rank, File file) {
        matrix[rank] += PiRank{file} & PiMask{pi};
    }

    void add(Pi pi, Square sq) {
        add(pi, Rank(sq), File(sq));
    }

    bool has(Pi pi, Square sq) const {
        return (matrix[Rank(sq)] & PiRank{File(sq)} & PiMask{pi}).any();
    }

    const PiRank& operator[] (Rank rank) const {
        return matrix[rank];
    }

    PiRank& operator[] (Rank rank) {
        return matrix[rank];
    }

    //pieces affecting the given square
    PiMask operator[] (Square sq) const {
        return matrix[Rank(sq)][File(sq)];
    }

    //bitboard of the given piece
    Bb operator[] (Pi pi) const {
        union {
            Bb::_t bb;
            Rank::arrayOf<BitRank::_t> br;
        };
        FOR_EACH(Rank, rank) {
            br[rank] = matrix[rank][pi];
        }
        return Bb{bb};
    }

    //bitboard of squares affected by all pieces
    Bb gather() const {
        union {
            Bb::_t bb;
            Rank::arrayOf<BitRank::_t> br;
        };
        FOR_EACH(Rank, rank) {
            br[rank] = matrix[rank].gather();
        }
        return Bb{bb};
    }

    void filter(Pi pi, Bb bb) {
        PiMask exceptPi{ PiMask::all() ^ PiMask{pi} };
        FOR_EACH(Rank, rank) {
            matrix[rank] &= PiRank{bb[rank]} | exceptPi;
        }
    }

    friend PiBb operator % (const PiBb& from, Bb bb) {
        PiBb result;
        FOR_EACH(Rank, rank) {
            result.matrix[rank] = from.matrix[rank] % PiRank{bb[rank]};
        }
        return result;
    }

    void operator &= (Bb bb) {
        FOR_EACH(Rank, rank) {
            matrix[rank] &= PiRank{bb[rank]};
        }
    }

    friend PiBb operator & (const PiBb& from, Bb bb) {
        PiBb result;
        FOR_EACH(Rank, rank) {
            result.matrix[rank] = from.matrix[rank] & PiRank{bb[rank]};
        }
        return result;
    }

    index_t popcount() const {
        index_t sum = 0;
        FOR_EACH(Rank, rank) {
            sum += ::popcount(matrix[rank]);
        }
        return sum;
    }
};

#endif
