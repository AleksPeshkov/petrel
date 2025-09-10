#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include "bitops.hpp"

using vu8x16_t = u8_t __attribute__((vector_size(16)));
using vu64x2_t = u64_t __attribute__((vector_size(16)));

constexpr u8_t u8(const vu8x16_t& v, int i) {
    union {
        vu8x16_t v;
        u8_t u8[16];
    } u;
    u.v = v;
    return u.u8[i];
}

constexpr u64_t u64(const vu64x2_t& v) {
    union {
        vu64x2_t v;
        u64_t u64[2];
    } u;
    u.v = v;
    return u.u64[0];
}

template <typename vector_type>
inline constexpr vector_type shufflevector(vector_type vector, vector_type mask) {
#if __clang__
    return __builtin_shufflevector(vector, mask);
#else
    return __builtin_shuffle(vector, mask);
#endif
}

inline int popcount(vu64x2_t v) {
    return popcount(v[0]) + popcount(v[1]);
}

inline int mask(vu8x16_t v) {
    return __builtin_ia32_pmovmskb128(v);
}

inline bool equals(const vu8x16_t& a, const vu8x16_t& b) {
    return mask(a == b) == 0xffffu;
}

constexpr vu8x16_t all(u8_t i) { return vu8x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }

#endif
