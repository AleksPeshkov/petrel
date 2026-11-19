#ifndef PI_BB_H
#define PI_BB_H

#include "simd256.hpp"
#include "Bb.hpp"
#include "PiMask.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"

class PiBb {
    using _t = u64_t __attribute__((vector_size(128)));
    _t v_{}; // array <Bb, Pi>

public:
    constexpr Bb bb(Pi pi) const {
        return Bb{ std::bit_cast<std::array<u64_t, 16>>(v_)[+pi] };
    }

    constexpr void set(Pi pi, Bb bb) {
        auto v = std::bit_cast<std::array<u64_t, 16>>(v_);
        v[+pi] = bb.v();
        v_ = std::bit_cast<_t>(v);
        return;
    }

    constexpr bool has(Pi pi, Square sq) const { return bb(pi).has(sq); }
    constexpr void clear(Pi pi, Square sq) { set(pi, bb(pi) - Bb{sq}); }
    constexpr void add(Pi pi, Square sq) { set(pi, bb(pi) + Bb{sq}); }

    constexpr void operator &= (Bb bb) { v_ &= splat<_t>(bb.v()); }
    constexpr void operator %= (Bb bb) { v_ &= splat<_t>(~bb.v()); }
    constexpr void filter(Pi pi, Bb b) { set(pi, bb(pi) & b); }

    PiMask piMask(Square sq) const {
        auto bit{ splat<_t>(Bb{sq}.v()) };
        auto mask{ (v_ & bit) == bit };

        using u8x128_t = u8_t __attribute__((vector_size(128)));
        auto bytes = std::bit_cast<u8x128_t>(mask);

        u8x16_t pieces = __builtin_shufflevector(bytes, bytes,
            0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120
        );

        return PiMask{pieces};
    }

    constexpr int popcount() const {
        int sum = 0;
        for (auto bb : std::bit_cast<std::array<u64_t, 16>>(v_)) {
            sum += Bb{bb}.popcount();
        }
        return sum;
    }

// used only in case of PiBb PositionSide::attacks_:

    constexpr void clear(Pi pi) { set(pi, Bb{}); }

    Bb bb() const {
        #ifdef __clang__
            return Bb{__builtin_reduce_or(v_)};
        #else
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return Bb{(v[Is] | ...)};
            }(std::make_index_sequence<16>{});
        #endif
    }
};

#pragma GCC diagnostic pop

#endif
