#ifndef PI_BB_H
#define PI_BB_H

#include "bitops256.hpp"
#include "Bb.hpp"
#include "PiMask.hpp"

using u32x4_t = u32_t __attribute__((vector_size(16)));

inline u64x2_t unpack2_lo32(u32x4_t a, u32x4_t b) {
    return __builtin_shufflevector(a, b, 0, 4, -1, -1 );
}

inline u8x16_t unpack2_lo64(u64x2_t a, u64x2_t b) {
    return __builtin_shufflevector(a, b, 0, 2 );
}

inline u8x16_t cast256_128(u64x4_t i256) {
#if USE_AVX2
        return _mm256_castsi256_si128(i256);
#else
        union {
            u64x4_t u64x4;
            u8x16_t u8x16;
        };
        u64x4 = i256;
        return u8x16;
#endif
}

class CACHE_ALIGN PiBb {
    union {
        u64x4_t u64x4[4];
        array <Bb, Pi> bb_;
    };

    constexpr void filter(u64_t bb) {
        u64x4_t bb4 = x4(bb);
        for (auto& v : u64x4) { v &= bb4; }
    }
public:
    constexpr PiBb() { for (auto& v : u64x4) { v = x4(0); } }

    void setAttacks(const PiBb& attacks) {
        for (auto i : range<4>()) { u64x4[i] = attacks.u64x4[i]; }
    }

    constexpr Bb bb(Pi pi) const { return bb_[pi]; }
    constexpr void set(Pi pi, Bb bb) { bb_[pi] = bb; }

    constexpr bool has(Pi pi, Square sq) const { return bb_[pi].has(sq); }
    constexpr void clear(Pi pi, Square sq) { bb_[pi] -= Bb{sq}; }
    constexpr void add(Pi pi, Square sq) {bb_[pi] += Bb{sq}; }

    constexpr void operator &= (Bb bb) { filter(bb.v()); }
    constexpr void operator %= (Bb bb) { filter(~bb.v()); }
    constexpr void filter(Pi pi, Bb bb) { bb_[pi] &= bb; }

    PiMask piMask(Square sq) const {
        Rank rank{sq.rank()};

        using i8x32_t =  i8_t __attribute__((vector_size(32)));
        i8x32_t rankMask{
            static_cast<i8_t>(+rank),
            static_cast<i8_t>(+rank+8),
            static_cast<i8_t>(+rank+16),
            static_cast<i8_t>(+rank+24),
            -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
        };

        auto a32 = cast256_128( shuffle(u64x4[0], rankMask) );
        auto b32 = cast256_128( shuffle(u64x4[1], rankMask) );
        auto c32 = cast256_128( shuffle(u64x4[2], rankMask) );
        auto d32 = cast256_128( shuffle(u64x4[3], rankMask) );

        auto a64 = unpack2_lo32(a32, b32);
        auto b64 = unpack2_lo32(c32, d32);
        auto r16 = unpack2_lo64(a64, b64);

        auto fileVector = x16(1 << +sq.file());
        return PiMask{ (r16 & fileVector) == fileVector };
    }

    constexpr int popcount() const {
        int sum = 0;
        for (auto bb : bb_) { sum += bb.popcount(); }
        return sum;
    }

// used only in case of PiBb PositionSide::attacks_:

    constexpr void clear(Pi pi) { bb_[pi] = {}; }

    Bb bb() const {
        auto a64 = u64x4[0] | u64x4[1] | u64x4[2] | u64x4[3];

#ifdef __clang__
        return Bb{__builtin_reduce_or(a64)};
#else
        return Bb{a64[0] | a64[1] | a64[2] | a64[3]};
#endif
    }
};

#endif
