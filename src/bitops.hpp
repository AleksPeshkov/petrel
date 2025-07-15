#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#include <cassert>
#include <climits>
#include <cstdint>
#include <type_traits>
#include "types.hpp"

#define INLINE       inline __attribute__((__always_inline__))
#define CACHE_ALIGN  __attribute__((__aligned__(64)))

template <typename T>
constexpr T universe() { return static_cast<T>(~static_cast<T>(0)); }

template <typename T, typename N>
constexpr T small_cast(N n) { return static_cast<T>(n & static_cast<N>(universe<T>())); }

template <typename T, typename N>
constexpr T singleton(N n) { return static_cast<T>(static_cast<T>(1u) << static_cast<T>(n)); }

// clears the lowest unset bit
template <typename T>
constexpr T clearFirst(T n) { return n & static_cast<T>(n-1); }

template <typename T>
constexpr bool isSingleton(T n) { return (n != 0) && (::clearFirst(n) == 0); }

constexpr inline std::uint64_t rotateleft(std::uint64_t b, index_t n) {
    return b << n | b >> (64 - n);
}

constexpr inline std::uint64_t byteswap(std::uint64_t b) {
    return __builtin_bswap64(b);
}

INLINE index_t bsf(u32_t b) {
    assert (b != 0);
    return static_cast<index_t>(__builtin_ctz(b));
}

INLINE index_t bsr(std::uint32_t b) {
    assert (b != 0);
    index_t i;
    __asm__ ( "bsrl %1, %0" : "=Ir" (i) : "rm" (b) : "cc" );
    return i;
}

INLINE index_t bsf(std::uint64_t b) {
    assert (b != 0);
    return static_cast<index_t>(__builtin_ctzll(b));
}

INLINE index_t bsr(std::uint64_t b) {
    assert (b != 0);
    __asm__ ( "bsrq %1, %0" : "=Jr" (b) : "rm" (b) : "cc" );
    return static_cast<index_t>(b);
}

#endif
