#ifndef BIT_ARRAY_128_HPP
#define BIT_ARRAY_128_HPP

#include "bitops128.hpp"
#include "BitArray.hpp"

template <>
struct BitArrayOps<u8x16_t> {
    static bool equals(const u8x16_t& a, const u8x16_t& b) { return ::equals(a.i128, b.i128); }
    static constexpr void and_assign(u8x16_t& a, const u8x16_t& b) { a.i128 &= b.i128; }
    static constexpr void or_assign(u8x16_t& a, const u8x16_t& b) { a.i128 |= b.i128; }
    static constexpr void xor_assign(u8x16_t& a, const u8x16_t& b) { a.i128 ^= b.i128; }
};

#endif
