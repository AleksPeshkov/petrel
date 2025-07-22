#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include <tmmintrin.h>
#include "bitops.hpp"

typedef __m128i i128_t;

inline index_t popcount(i128_t n) {
    auto lo = static_cast<u64_t>(_mm_cvtsi128_si64(n));
    auto hi = static_cast<u64_t>(_mm_cvtsi128_si64(_mm_unpackhi_epi64(n, n)));
    return popcount(lo) + popcount(hi);
}

inline bool equals(const i128_t& a, const i128_t& b) {
    return _mm_movemask_epi8(_mm_cmpeq_epi8(a, b)) == 0xffffu;
}

union u8x16_t {
    u8_t u8[16];
    i128_t i128;

    constexpr operator const i128_t&() const { return i128; }
};

#endif
