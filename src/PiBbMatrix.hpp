#ifndef PI_BB_H
#define PI_BB_H

#include "bitops256.hpp"
#include "Bb.hpp"
#include "PiMask.hpp"

class PiRank : public BitArray<PiRank, u8x16_t> {
public:
    constexpr PiRank () : BitArray{::sameVector(0)} {}
    constexpr explicit PiRank (_t v) : BitArray{v} {}
    constexpr explicit PiRank (BitRank br) : PiRank{::sameVector(br.v())} {}
    constexpr explicit PiRank (PiMask m) : PiRank{m.v()} {}
    constexpr explicit PiRank (File file) : PiRank{BitRank{static_cast<BitRank::_t>(1 << +file)}} {}

    BitRank reduce() const {
#ifdef __clang__
        return BitRank{__builtin_reduce_or(v_)};
#else
        u8_t r  = v_[0] | v_[1] | v_[2] | v_[3] | v_[4] | v_[5] | v_[6] | v_[7]
            | v_[8] | v_[9] | v_[10] | v_[11] | v_[12] | v_[13] | v_[14] | v_[15];
        return BitRank{r};
#endif
    }

    constexpr PiMask piMask(File file) const {
        _t file_vector = PiRank{file}.v();
        return PiMask{ (v_ & file_vector) == file_vector };
    }

    //_mm_blendv_epi8
    constexpr PiRank& blend(Pi pi, BitRank br) {
        v_ = PiMask{pi}.v() ? PiRank{br}.v() : v_;
        return *this;
    }
};

/// array of 8 PiRank
class CACHE_ALIGN PiBbMatrix {
    array<PiRank, Rank> v_;

public:
    constexpr PiBbMatrix () {}
    constexpr void clear() { for (auto& piRank : v_) { piRank = {}; } }

    constexpr void clear(Pi pi) {
        for (auto& piRank : v_) {
            piRank %= PiRank{PiMask{pi}};
        }
    }

    constexpr void set(Pi pi, Bb bb) {
        for (auto rank : range<Rank>()) {
            v_[rank].blend(pi, bb.bitRank(rank));
        }
    }

    // pieces affecting the given square
    constexpr PiMask piMask(Square sq) const {
        return v_[sq.rank()].piMask(sq.file());
    }

    // bitboard of squares affected by all pieces
    constexpr Bb bb() const {
        array<BitRank, Rank> br;
        for (auto rank : range<Rank>()) {
            br[rank] = v_[rank].reduce();
        }
        return Bb{std::bit_cast<Bb::_t>(br)};
    }
};

constexpr u64x4_t x4(u64_t n) { return u64x4_t{n, n, n, n}; }

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
    constexpr PiBb() {}
    constexpr void clear() { for (auto& v : u64x4) { v = x4(0); } }

    void setAttacks(const PiBb& attacks) {
        for (auto i : range<4>()) { u64x4[i] = attacks.u64x4[i]; }
    }

    void setAttacks(const PiBbMatrix& attacks) {
        ::transpose(u64x4, reinterpret_cast<const u8x32_t*>(&attacks));
    }

    constexpr Bb bb(Pi pi) const { return bb_[pi]; }
    constexpr void set(Pi pi, Bb bb) { bb_[pi] = bb; }

    constexpr bool has(Pi pi, Square sq) const { return bb_[pi].has(sq); }
    constexpr void clear(Pi pi, Square sq) { bb_[pi] -= Bb{sq}; }
    constexpr void add(Pi pi, Square sq) {bb_[pi] += Bb{sq}; }

    constexpr void operator &= (Bb bb) { filter(bb.v()); }
    constexpr void operator %= (Bb bb) { filter(~bb.v()); }
    constexpr void filter(Pi pi, Bb bb) { bb_[pi] &= bb; }

    // much slower than PiBbMatrix::piMask()
    PiMask piMask(Square sq) const {
        Rank rank{sq.rank()};
        i8x32_t rankMask{
            static_cast<i8_t>(+rank),
            static_cast<i8_t>(+rank+8),
            static_cast<i8_t>(+rank+16),
            static_cast<i8_t>(+rank+24),
            -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
        };

        auto a32 = shuffle(static_cast<i8x32_t>(u64x4[0]), rankMask);
        auto b32 = shuffle(static_cast<i8x32_t>(u64x4[1]), rankMask);
        auto c32 = shuffle(static_cast<i8x32_t>(u64x4[2]), rankMask);
        auto d32 = shuffle(static_cast<i8x32_t>(u64x4[3]), rankMask);

        auto a64 = unpack_lo32(a32, b32);
        auto b64 = unpack_lo32(c32, d32);

#if USE_AVX2
        u8x32_t r32 = unpack_lo64(a64, b64);
        u8x16_t r16 = _mm256_castsi256_si128(r32);
#else
        union {
            u8x32_t r32;
            u8x16_t r16;
        };
        u8x32_t r32 = unpack_lo64(a64, b64);
#endif

        PiRank piRank{r16};
        return piRank.piMask(sq.file());
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
