#ifndef BIT_ARRAY_128_HPP
#define BIT_ARRAY_128_HPP

#include "bitops128.hpp"
#include "BitArray.hpp"

template <>
struct BitArrayOps<vu8x16_t> {
    typedef vu8x16_t _t;
    typedef const _t& Arg;
    static bool equals(Arg a, Arg b) { return ::equals(a, b); }
    static constexpr void and_assign(_t& a, Arg b) { a &= b; }
    static constexpr void or_assign(_t& a, Arg b) { a |= b; }
    static constexpr void xor_assign(_t& a, Arg b) { a ^= b; }
};

#endif
