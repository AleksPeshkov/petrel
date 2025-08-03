#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include <tmmintrin.h>
#include "bitops.hpp"

typedef __m128i i128_t;
typedef u8_t  vu8x16_t __attribute__((vector_size(16)));
typedef u64_t vu64x2_t __attribute__((vector_size(16)));

template <typename vector_type>
inline constexpr vector_type shufflevector(vector_type vector, vector_type mask) {
#if __clang__
    return __builtin_shufflevector(vector, mask);
#else
    return __builtin_shuffle(vector, mask);
#endif
}

inline index_t popcount(i128_t n) {
    auto lo = static_cast<u64_t>(_mm_cvtsi128_si64(n));
    auto hi = static_cast<u64_t>(_mm_cvtsi128_si64(_mm_unpackhi_epi64(n, n)));
    return popcount(lo) + popcount(hi);
}

template <typename vector_type>
inline bool equals(const vector_type& a, const vector_type& b) {
    return _mm_movemask_epi8(_mm_cmpeq_epi8(a, b)) == 0xffffu;
}

typedef u8_t au8x16_t [16];

union u8x16_t {
    vu8x16_t vu8x16;
    au8x16_t u8;
    i128_t  i128;

    constexpr operator const i128_t&   () const { return i128; }
    constexpr operator const vu8x16_t& () const { return vu8x16; }
    constexpr operator const au8x16_t& () const { return u8; }
};

#endif
