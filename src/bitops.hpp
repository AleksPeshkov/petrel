#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#include <climits>
#include <cstdint>
#include <type_traits>
#include "assert.hpp"
#include "types.hpp"

#define CACHE_ALIGN __attribute__((__aligned__(64)))
#define PACKED __attribute__((packed))

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

constexpr inline std::uint64_t rotateleft(std::uint64_t b, int n) {
    if  (n == 0) { return b; }
    return b << n | b >> (64 - n);
}

constexpr inline std::uint64_t byteswap(std::uint64_t b) {
    return __builtin_bswap64(b);
}

// the least significant bit in a non-zero bitset
constexpr inline int lsb(std::uint32_t b) {
    assert (b != 0);
    return __builtin_ctz(b);
}

// the most significant bit in a non-zero bitset
constexpr inline int msb(std::uint32_t b) {
    assert (b != 0);
    return 31 ^ __builtin_clz(b);
}

// the least significant bit in a non-zero bitset
constexpr inline int lsb(std::uint64_t b) {
    assert (b != 0);
    return __builtin_ctzll(b);
}

// the most significant bit in a non-zero bitset
constexpr inline int msb(std::uint64_t b) {
    assert (b != 0);
    return 63 ^ __builtin_clzll(b);
}

template <typename T>
constexpr T round(T n) {
    assert (n > 0);
    return ::singleton<decltype(n)>(::msb(n));
}

constexpr inline int popcount(std::uint32_t b) {
    return __builtin_popcount(b);
}

constexpr inline int popcount(std::uint64_t b) {
    return __builtin_popcountll(b);
}

#endif
