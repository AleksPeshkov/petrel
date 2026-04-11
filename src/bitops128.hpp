#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include "bitops.hpp"

using vu8x16_t = u8_t __attribute__((vector_size(16)));
using vu64x2_t = u64_t __attribute__((vector_size(16)));

constexpr u64_t u64(vu64x2_t v, int i = 0) {
    return std::bit_cast<std::array<u64_t, 2>>(v)[i];
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

#endif // BIT_OPS_128_HPP
