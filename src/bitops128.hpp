#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#if defined __aarch64__

#include <arm_neon.h>
#define NEON_VECTOR

#endif

#include "bitops.hpp"

using vu8x16_t = u8_t __attribute__((vector_size(16)));
using vu64x2_t = u64_t __attribute__((vector_size(16)));

constexpr u8_t u8(const vu8x16_t& vec, int i) {
    union {
        vu8x16_t vu8x16;
        u8_t u8[16];
    } u;
    u.vu8x16 = vec;
    return u.u8[i];
}

constexpr vu8x16_t all(u8_t i) { return vu8x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }

constexpr u64_t u64(vu64x2_t vec, int i = 0) {
    union {
        vu64x2_t vu64x2;
        u64_t u64[2];
    } u;
    u.vu64x2 = vec;
    return u.u64[i];
}

constexpr int popcount(vu64x2_t v) {
    return popcount(::u64(v, 0)) + popcount(::u64(v, 1));
}

template <typename value_type>
constexpr value_type shufflevector(value_type vector, value_type mask) {
#if __clang__
    return __builtin_shufflevector(vector, mask);
#else
    return __builtin_shuffle(vector, mask);
#endif
}

#ifndef NEON_VECTOR

constexpr int mask(vu8x16_t v) {
    if (std::is_constant_evaluated()) {
        int result = 0;
        for (int i = 0; i < 16; ++i) {
            result |= (::u8(v, i) >> 7) << i;
        }
        return result;
    /*
        // much faster, but somehow broke clang linker
        // https://stackoverflow.com/questions/74722950/convert-vector-compare-mask-into-bit-mask-in-aarch64-simd-or-arm-neon
        int hi = (::u64(v, 1) * U64(0x000103070f1f3f80)) >> 56;
        int lo = (::u64(v, 0) * U64(0x000103070f1f3f80)) >> 56;
        return (hi << 8) + lo;
    */
    } else {
        return __builtin_ia32_pmovmskb128(v);
    }
}

constexpr bool equals(vu8x16_t a, vu8x16_t b) {
    return mask(a == b) == 0xffffu;
}

#else

// https://community.arm.com/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
constexpr u64_t mask4(vu8x16_t v) {
    if (std::is_constant_evaluated()) {
        u64_t result = 0;
        for (int i = 0; i < 16; ++i) {
            result |= static_cast<u64_t>(::u8(v, i) >> 4) << (i << 2);
        }
        return result;
    } else {
        return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(v), 4)), 0);
    }
}

constexpr bool equals(vu8x16_t a, vu8x16_t b) {
    return mask4(a == b) == U64(0xffff'ffff'ffff'ffff);
}

#endif // NEON_VECTOR

#endif // BIT_OPS_128_HPP
