#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>

typedef std::int8_t i8_t;
typedef std::uint8_t u8_t;

typedef std::int16_t i16_t;
typedef std::uint16_t u16_t;

typedef std::int32_t i32_t;
typedef std::uint32_t u32_t;

typedef std::int64_t i64_t;
typedef std::uint64_t u64_t;

#if defined _WIN32
#   define ULL(number) number##ull
#else
#   define ULL(number) number##ul
#endif

#endif
