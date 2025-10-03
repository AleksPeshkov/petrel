#ifndef BIT_OPS_HPP
#define BIT_OPS_HPP

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include "assert.hpp"

using std::size_t;

using i8_t = std::int8_t;
using u8_t = std::uint8_t;

using i16_t = std::int16_t;
using u16_t = std::uint16_t;

using i32_t = std::int32_t;
using u32_t = std::uint32_t;

using i64_t = std::int64_t;
using u64_t = std::uint64_t;

#if defined _WIN32
#   define U64(number) number##ull
#else
#   define U64(number) number##ul
#endif

#define CACHE_ALIGN __attribute__((__aligned__(64)))
#define PACKED __attribute__((packed))

template <typename T>
constexpr T universe() {
    return static_cast<T>(~static_cast<T>(0));
}

template <typename T, typename N>
constexpr T small_cast(N n) {
    return static_cast<T>(n & static_cast<N>(universe<T>()));
}

template <typename T, typename N>
constexpr T singleton(N n) {
    return static_cast<T>(static_cast<T>(1u) << static_cast<T>(n));
}

// clears the lowest unset bit
template <typename T>
constexpr T clearFirst(T n) {
    return n & static_cast<T>(n-1);
}

template <typename T>
constexpr bool isSingleton(T n) {
    return std::has_single_bit(n);
}

constexpr u64_t rotateleft(u64_t b, int n) {
    return std::rotl(b, n);
}

constexpr u64_t byteswap(u64_t b) {
    return __builtin_bswap64(b);
}

// the least significant bit in a non-zero bitset
constexpr int lsb(u32_t b) {
    assert (b != 0);
    return std::countr_zero(b);
}

// the most significant bit in a non-zero bitset
constexpr int msb(u32_t b) {
    assert (b != 0);
    return 31 ^ std::countl_zero(b);
}

// the least significant bit in a non-zero bitset
constexpr int lsb(u64_t b) {
    assert (b != 0);
    return std::countr_zero(b);
}

// the most significant bit in a non-zero bitset
constexpr int msb(u64_t b) {
    assert (b != 0);
    return 63 ^ std::countl_zero(b);
}

template <typename T>
constexpr T round(T n) {
    assert (n > 0);
    return std::bit_floor(n);
}

constexpr inline int popcount(u32_t b) {
    return std::popcount(b);
}

constexpr inline int popcount(u64_t b) {
    return std::popcount(b);
}

#endif
