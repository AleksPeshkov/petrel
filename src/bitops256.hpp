#ifndef BITOPS256_HPP
#define BITOPS256_HPP

#ifdef __AVX2__
    #define USE_AVX2 1
    #include <immintrin.h>
#else
    #define USE_AVX2 0
#endif

#include "bitops128.hpp"

#endif
