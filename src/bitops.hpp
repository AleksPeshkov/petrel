#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#include <cassert>
#include <climits>
#include <cstdint>
#include <type_traits>
#include "types.hpp"

#define CACHE_ALIGN __attribute__((__aligned__(64)))
#define PACKED __attribute__((packed))

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
universe() { return static_cast<T>(~static_cast<T>(0)); }

#include <type_traits>
template <typename T, typename N>
constexpr typename std::enable_if
    <std::is_integral<T>::value && (std::is_integral<N>::value || std::is_enum<N>::value), T>::type
small_cast(N n) { return static_cast<T>(n & universe<T>()); }

#ifdef NDEBUG
    template <typename T, typename N>
    constexpr T narrow_cast(N n) { return small_cast<T>(n); }
#else
#   include <stdexcept>
    template <typename T, typename N>
    T narrow_cast(N n) {
        auto r = static_cast<T>(n);
        if (static_cast<N>(r) != n) {
            throw std::runtime_error("narrow_cast<>() failed");
        }
        return r;
    }
#endif

template <typename T, typename N>
constexpr T singleton(N n) { return static_cast<T>(static_cast<T>(1u) << static_cast<T>(n)); }

// clears the lowest unset bit
template <typename T>
constexpr T clearFirst(T n) { return n & static_cast<T>(n-1); }

template <typename T>
constexpr bool isSingleton(T n) { return (n != 0) && (::clearFirst(n) == 0); }

constexpr inline std::uint64_t rotateleft(std::uint64_t b, index_t n) {
    if  (n == 0) { return b; }
    return b << n | b >> (64 - n);
}

constexpr inline std::uint64_t byteswap(std::uint64_t b) {
    return __builtin_bswap64(b);
}

// the least significant bit in a non-zero bitset
constexpr inline index_t lsb(u32_t b) {
    assert (b != 0);
    return static_cast<index_t>(__builtin_ctz(b));
}

// the most significant bit in a non-zero bitset
constexpr inline index_t msb(std::uint32_t b) {
    assert (b != 0);
    return static_cast<index_t>(31 ^ __builtin_clz(b));
}

// the least significant bit in a non-zero bitset
constexpr inline index_t lsb(std::uint64_t b) {
    assert (b != 0);
    return static_cast<index_t>(__builtin_ctzll(b));
}

// the most significant bit in a non-zero bitset
constexpr inline index_t msb(std::uint64_t b) {
    assert (b != 0);
    return static_cast<index_t>(63 ^ __builtin_clzll(b));
}

template <typename T>
constexpr T round(T n) {
    assert (n > 0);
    return ::singleton<decltype(n)>(::msb(n));
}

constexpr inline index_t popcount(std::uint32_t b) {
    return static_cast<index_t>(__builtin_popcount(b));
}

constexpr inline index_t popcount(std::uint64_t b) {
    return static_cast<index_t>(__builtin_popcountll(b));
}

#endif
