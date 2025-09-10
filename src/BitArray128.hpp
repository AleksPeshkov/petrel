#ifndef BIT_ARRAY_128_HPP
#define BIT_ARRAY_128_HPP

#include "bitops128.hpp"
#include "BitArray.hpp"

template <>
struct BitArrayOps<vu8x16_t> {
    using _t = vu8x16_t;
    using Arg = const _t&;
    static bool equals(Arg a, Arg b) { return ::equals(a, b); }
};

#endif
