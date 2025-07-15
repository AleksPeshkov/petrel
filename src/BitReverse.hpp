#ifndef BIT_REVERSE_HPP
#define BIT_REVERSE_HPP

#include "bitops128.hpp"
#include "VectorOfAll.hpp"

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

#endif
