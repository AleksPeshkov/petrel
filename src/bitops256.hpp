#ifndef BITOPS256_HPP
#define BITOPS256_HPP

#ifdef __AVX2__
    #define USE_AVX2 1
    #include <immintrin.h>
#else
    #define USE_AVX2 0
#endif

#include "bitops128.hpp"

using u64x4_t = u64_t __attribute__((vector_size(32)));

constexpr u64x4_t x4(u64_t n) { return u64x4_t{n, n, n, n}; }

#endif
