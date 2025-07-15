#ifndef VECTOR_BIT_COUNT_HPP
#define VECTOR_BIT_COUNT_HPP

#include "bitops128.hpp"
#include "VectorOfAll.hpp"

class VectorBitCount {
public:
    typedef i128_t _t;

private:
    const u8x16_t nibbleCount = {{ 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, }};

public:
    _t bytes(_t v) const {
        constexpr auto& nibbleMask = ::vectorOfAll[0x0f];
        _t lo =  _mm_and_si128(v, nibbleMask);
        _t hi =  _mm_and_si128(_mm_srli_epi16(v, 4), nibbleMask);
        lo = _mm_shuffle_epi8(nibbleCount, lo);
        hi = _mm_shuffle_epi8(nibbleCount, hi);
        return _mm_add_epi8(lo, hi);
    }

    static index_t total(_t v) {
        _t lo = _mm_sad_epu8(v, _t{0,0});
        _t hi = _mm_unpackhi_epi64(lo, _t{0,0});
        lo = _mm_add_epi64(lo, hi);
        return static_cast<index_t>(_mm_cvtsi128_si32(lo));
    }

    index_t operator() (_t v) const {
        return total(bytes(v));
    }
};

extern const VectorBitCount bitCount;

#endif
