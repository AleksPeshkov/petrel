#ifndef BIT_OPS_128_HPP
#define BIT_OPS_128_HPP

#include <array>
#include "bitops.hpp"

using u8x16_t =  u8_t __attribute__((vector_size(16)));
using u64x2_t = u64_t __attribute__((vector_size(16)));

constexpr u8x16_t x16(u8_t b) { return u8x16_t{ b,b,b,b, b,b,b,b, b,b,b,b, b,b,b,b }; }

template <typename vector_type, typename mask_type>
constexpr vector_type shuffle(vector_type vector, mask_type mask) {
#ifdef __clang__
    return __builtin_shufflevector(static_cast<mask_type>(vector), mask);
#else
    return __builtin_shuffle(static_cast<mask_type>(vector), mask);
#endif
}

#endif // BIT_OPS_128_HPP
