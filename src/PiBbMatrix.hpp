#ifndef PI_BB_H
#define PI_BB_H

#include "Bb.hpp"
#include "PiMask.hpp"

class PiRank : public BitArray<PiRank, vu8x16_t> {
    using Base = BitArray<PiRank, vu8x16_t>;

public:
    using Base::Base;
    constexpr PiRank () : Base{::all(0)} {}
    constexpr explicit PiRank (BitRank br) : Base{::vectorOfAll[br.v()]} {}
    constexpr explicit PiRank (PiMask m) : Base{m.v()} {}
    constexpr explicit PiRank (File file) : PiRank{BitRank{file}} {}
    constexpr explicit PiRank (Pi pi) : PiRank{PiMask{pi}} {}
    constexpr PiRank(Bb bb, Rank rank) : PiRank{BitRank{bb, rank}} {}
    constexpr PiRank(Pi pi, File file) : PiRank{PiRank{file} & PiRank{pi}} {}
    constexpr PiRank(Pi pi, BitRank br) : PiRank{PiRank{br} & PiRank{pi}} {}

    BitRank bb() const {
        u8_t r  = v_[0] | v_[1] | v_[2] | v_[3] | v_[4] | v_[5] | v_[6] | v_[7]
            | v_[8] | v_[9] | v_[10] | v_[11] | v_[12] | v_[13] | v_[14] | v_[15];
        return BitRank{r};
    }

    constexpr BitRank bitRank(Pi pi) const {
        return BitRank{ ::u8(v_, pi.v()) };
    }

    PiMask piMask(File file) const {
        _t file_vector = PiRank{file}.v();
        return PiMask{ (v_ & file_vector) == file_vector };
    }
};

/// array of 8 PiRank
class PiBbMatrix {
    Rank::arrayOf<PiRank> v_;

public:
    constexpr void clear() {
        for (auto& piRank : v_) {
            piRank.clear();
        }
    }

    constexpr void clear(Pi pi, Square sq) {
        v_[Rank{sq}] -= PiRank{pi, File{sq}};
    }

    constexpr void clear(Pi pi) {
        for (auto& piRank : v_) {
            piRank %= PiRank{pi};
        }
    }

    constexpr void set(Pi pi, Rank rank, BitRank br) {
        //_mm_blendv_epi8
        v_[rank] = (v_[rank] % PiRank{pi}) + PiRank{pi, br};
    }

    constexpr void set(Pi pi, Bb bb) {
        for (auto rank : range<Rank>()) {
            set(pi, rank, BitRank{bb, rank});
        }
    }

    constexpr void add(Pi pi, File file, Rank rank) {
        v_[rank] += PiRank{pi, file};
    }

    constexpr void add(Pi pi, Square sq) {
        add(pi, File{sq}, Rank{sq});
    }

    constexpr bool has(Pi pi, Square sq) const {
        return (v_[Rank{sq}] & PiRank{pi, File{sq}}).any();
    }

    constexpr const PiRank& operator[] (Rank rank) const {
        return v_[rank];
    }

    constexpr PiRank& operator[] (Rank rank) {
        return v_[rank];
    }

    // pieces affecting the given square
    PiMask piMask(Square sq) const {
        return v_[Rank{sq}].piMask(File{sq});
    }

    // bitboard of the given piece
    constexpr Bb bb(Pi pi) const {
        Rank::arrayOf<BitRank> br;
        for (auto rank : range<Rank>()) {
            br[rank] = v_[rank].bitRank(pi);
        }
        return Bb{std::bit_cast<Bb::_t>(br)};
    }

    // bitboard of squares affected by all pieces
    constexpr Bb bb() const {
        Rank::arrayOf<BitRank> br;
        for (auto rank : range<Rank>()) {
            br[rank] = v_[rank].bb();
        }
        return Bb{std::bit_cast<Bb::_t>(br)};
    }

    void filter(Pi pi, Bb bb) {
        PiRank exceptPi{ PiRank{BitRank{0xff}} ^ PiRank{pi} };
        for (auto rank : range<Rank>()) {
            v_[rank] &= PiRank{bb, rank} | exceptPi;
        }
    }

    friend constexpr PiBbMatrix operator % (const PiBbMatrix& from, Bb bb) {
        PiBbMatrix result;
        for (auto rank : range<Rank>()) {
            result.v_[rank] = from.v_[rank] % PiRank{bb, rank};
        }
        return result;
    }

    constexpr void operator &= (Bb bb) {
        for (auto rank : range<Rank>()) {
            v_[rank] &= PiRank{bb, rank};
        }
    }

    friend constexpr PiBbMatrix operator & (const PiBbMatrix& from, Bb bb) {
        PiBbMatrix result;
        for (auto rank : range<Rank>()) {
            result.v_[rank] = from.v_[rank] & PiRank{bb, rank};
        }
        return result;
    }

    constexpr bool none() const {
        for (auto piRank : v_) {
            if (piRank.any()) { return false; }
        }
        return true;
    }

    constexpr int popcount() const {
        int sum = 0;
        for (auto piRank : v_) {
            sum += ::popcount(piRank.v());
        }
        return sum;
    }
};

#endif
