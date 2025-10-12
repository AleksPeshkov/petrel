#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include "bitops.hpp"

using vu8x16_t = u8_t __attribute__((vector_size(16)));
using vu64x2_t = u64_t __attribute__((vector_size(16)));

#if defined __aarch64__

#include <arm_neon.h>
#define NEON_VECTOR

#elif defined __x86_64__

#define SSE_VECTOR

#endif

constexpr u8_t u8(const vu8x16_t& v, int i) {
    union {
        vu8x16_t v;
        u8_t u8[16];
    } u;
    u.v = v;
    return u.u8[i];
}

constexpr vu8x16_t all(u8_t i) { return vu8x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }

constexpr u64_t u64(const vu64x2_t& v, int i = 0) {
    union {
        vu64x2_t v;
        u64_t u64[2];
    } u;
    u.v = v;
    return u.u64[i];
}

constexpr int popcount(vu64x2_t v) {
    return popcount(::u64(v, 0)) + popcount(::u64(v, 1));
}

template <typename vector_type>
constexpr vector_type shufflevector(vector_type vector, vector_type mask) {
#if __clang__
    return __builtin_shufflevector(vector, mask);
#else
    return __builtin_shuffle(vector, mask);
#endif
}

#if defined SSE_VECTOR

inline int mask(vu8x16_t v) {
    return __builtin_ia32_pmovmskb128(v);
}

inline bool equals(const vu8x16_t& a, const vu8x16_t& b) {
    return mask(a == b) == 0xffffu;
}

#elif defined NEON_VECTOR

// https://community.arm.com/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
inline u64_t mask4(vu64x2_t v) {
    return ::u64(vshrn_n_u16(v, 4));

/*
    // vshrn_n_u16() emulation (only for test purposes)
    u64_t result = 0;
    for (int i = 0; i < 16; ++i) {
        result |= static_cast<u64_t>(::u8(v, i) >> 4) << (i << 2);
    }
    return result;
*/
}

inline bool equals(const vu8x16_t& a, const vu8x16_t& b) {
    return mask4(a == b) == U64(0xffff'ffff'ffff'ffff);
}

#else

constexpr int mask(vu8x16_t v) {
    int result = 0;
    for (int i = 0; i < 16; ++i) {
        if (::u8(v, i) & 0x80) {
            result |= (1 << i);
        }
    }
    return result;

/*
    // faster, but somehow broke clang linker
    // https://stackoverflow.com/questions/74722950/convert-vector-compare-mask-into-bit-mask-in-aarch64-simd-or-arm-neon
    int hi = (::u64(v, 1) * U64(0x000103070f1f3f80)) >> 56;
    int lo = (::u64(v, 0) * U64(0x000103070f1f3f80)) >> 56;
    return (hi << 8) | lo;
*/
}

constexpr bool equals(const vu8x16_t& a, const vu8x16_t& b) {
    return mask(a == b) == 0xffffu;
}

#endif // NEON_VECTOR

#endif // BIT_OPS_128_HPP
