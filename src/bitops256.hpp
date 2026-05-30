#ifndef BITOPS256_HPP
#define BITOPS256_HPP

#ifdef __AVX2__
    #define USE_AVX2 1
    #include <immintrin.h>
#else
    #define USE_AVX2 0
#endif

#include "bitops128.hpp"

using  u8x32_t =  u8_t __attribute__((vector_size(32)));
using  i8x32_t =  i8_t __attribute__((vector_size(32)));
using u16x16_t = u16_t __attribute__((vector_size(32)));
using u32x8_t  = u32_t __attribute__((vector_size(32)));
using u64x4_t  = u64_t __attribute__((vector_size(32)));

inline u16x16_t repack_8(u8x32_t v) {
    return __builtin_shufflevector(v, v,
        0,16, 1,17, 2,18, 3,19, 4,20, 5,21, 6,22, 7,23,
        8,24, 9,25,10,26,11,27,12,28,13,29,14,30,15,31
    );
}

inline u32x8_t unpack_lo16(u16x16_t a, u16x16_t b) {
    return __builtin_shufflevector(a, b,
        0, 16, 1, 17, 2, 18, 3, 19,
        4, 20, 5, 21, 6, 22, 7, 23
    );
}

inline u32x8_t unpack_hi16(u16x16_t a, u16x16_t b) {
    return __builtin_shufflevector(a, b,
        8, 24, 9, 25, 10, 26, 11, 27,
        12, 28, 13, 29, 14, 30, 15, 31
    );
}

inline u64x4_t unpack_lo32(u32x8_t a, u32x8_t b) {
    return __builtin_shufflevector(a, b,
        0,8, 1,9, 2,10, 3,11
    );
}

inline u64x4_t unpack_hi32(u32x8_t a, u32x8_t b) {
    return __builtin_shufflevector(a, b,
        4,12, 5,13, 6,14, 7,15
    );
}

inline u64x4_t unpack_lo64(u64x4_t a, u64x4_t b) {
    return __builtin_shufflevector(a, b,
        0, 4, 1, 5
    );
}

inline u64x4_t unpack_hi64(u64x4_t a, u64x4_t b) {
    return __builtin_shufflevector(a, b,
        2, 6, 3, 7
    );
}

// 8x u8x16_t -> 16x u64_t
inline void transpose(u64x4_t* to, const u8x32_t* from) {
    auto a16 = repack_8(from[0]);
    auto b16 = repack_8(from[1]);
    auto c16 = repack_8(from[2]);
    auto d16 = repack_8(from[3]);

    auto a32 = unpack_lo16(a16, b16);
    auto b32 = unpack_hi16(a16, b16);
    auto c32 = unpack_lo16(c16, d16);
    auto d32 = unpack_hi16(c16, d16);

    to[0] = unpack_lo32(a32, c32);
    to[1] = unpack_hi32(a32, c32);
    to[2] = unpack_lo32(b32, d32);
    to[3] = unpack_hi32(b32, d32);
}

#endif
