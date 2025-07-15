#ifndef PI_RANK_HPP
#define PI_RANK_HPP

#include "typedefs.hpp"
#include "Bb.hpp"
#include "VectorBitCount.hpp"
#include "VectorOfAll.hpp"
#include "PiBit.hpp"

struct PiRank : PiBit<PiRank, File> {
    typedef PiBit<PiRank, File> Base;
    constexpr PiRank () : Base() {}
    constexpr explicit PiRank (BitRank br) : Base(::vectorOfAll[br]) {}
    constexpr explicit PiRank (File f) : Base(::vectorOfAll[BitRank{f}]) {}
    constexpr PiRank (PiMask m) : Base(static_cast<PiMask::_t>(m)) {}

    BitRank gather() const {
        _t r = this->v;
        r |= _mm_unpackhi_epi64(r, r); //64
        r |= _mm_shuffle_epi32(r, _MM_SHUFFLE(1, 1, 1, 1)); //32
        r |= _mm_shufflelo_epi16(r, _MM_SHUFFLE(1, 1, 1, 1)); //16
        r |= _mm_srli_epi16(r, 8); //8
        return BitRank{small_cast<BitRank::_t>(_mm_cvtsi128_si32(r))};
    }

    constexpr BitRank operator [] (Pi pi) const {
        return BitRank{ get(pi) };
    }

    PiMask operator [] (File file) const {
        _t file_vector = ::vectorOfAll[BitRank{file}];
        return PiMask::equals(file_vector, this->v & file_vector);
    }

};

#endif
