#ifndef PI_RANK_HPP
#define PI_RANK_HPP

#include "typedefs.hpp"
#include "BitArray128.hpp"
#include "PiMask.hpp"
#include "VectorOfAll.hpp"

class PiRank : public BitArray<PiRank, u8x16_t> {
    typedef BitArray<PiRank, u8x16_t> Base;

    constexpr static u8x16_t vector(BitRank br) { return ::vectorOfAll[br]; }

public:
    using Base::Base;
    constexpr explicit PiRank (BitRank br) : Base{vector(br)} {}
    constexpr explicit PiRank (File f) : Base{vector(BitRank{f})} {}
    constexpr PiRank (PiMask m) : Base{m} {}

    constexpr operator const i128_t& () const { return v.i128; }

    BitRank gather() const {
        i128_t r = v.i128;
        r |= _mm_unpackhi_epi64(r, r); //64
        r |= _mm_shuffle_epi32(r, _MM_SHUFFLE(1, 1, 1, 1)); //32
        r |= _mm_shufflelo_epi16(r, _MM_SHUFFLE(1, 1, 1, 1)); //16
        r |= _mm_srli_epi16(r, 8); //8
        return BitRank{small_cast<BitRank::_t>(_mm_cvtsi128_si32(r))};
    }

    constexpr BitRank operator [] (Pi pi) const {
        return BitRank{ v.u8[pi] };
    }

    PiMask operator [] (File file) const {
        auto file_vector = vector(BitRank{file}).i128;
        return PiMask::equals(v.i128 & file_vector, file_vector);
    }

    constexpr void clear() { v.i128 = i128_t{0,0}; }
};

#endif
