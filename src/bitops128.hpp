#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include "bitops.hpp"

using vu8x16_t = u8_t __attribute__((vector_size(16)));
using vu64x2_t = u64_t __attribute__((vector_size(16)));

constexpr vu8x16_t vu8x16x(u8_t e) { return vu8x16_t{ e,e,e,e, e,e,e,e, e,e,e,e, e,e,e,e }; }

constexpr u64_t u64(vu64x2_t v, int i = 0) {
    return std::bit_cast<std::array<u64_t, 2>>(v)[i];
}

template <typename vector_type>
constexpr vector_type shufflevector(vector_type vector, vector_type mask) {
#ifdef __clang__
    return __builtin_shufflevector(vector, mask);
#else
    return __builtin_shuffle(vector, mask);
#endif
}

#endif // BIT_OPS_128_HPP
