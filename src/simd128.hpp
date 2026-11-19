#ifndef SIMD_128_HPP
#define SIMD_128_HPP

#include "bitops.hpp"
#include "simd.hpp"

using u8x16_t =  u8_t __attribute__((vector_size(16)));
using u32x4_t = u32_t __attribute__((vector_size(16)));
using u64x2_t = u64_t __attribute__((vector_size(16)));

inline u64x2_t unpack2_lo32(u32x4_t a, u32x4_t b) {
    return __builtin_shufflevector(a, b, 0, 4, -1, -1 );
}

inline u8x16_t unpack2_lo64(u64x2_t a, u64x2_t b) {
    return __builtin_shufflevector(a, b, 0, 2 );
}

#endif // BIT_OPS_128_HPP
