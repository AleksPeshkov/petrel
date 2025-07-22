#ifndef BB_HPP
#define BB_HPP

#include "io.hpp"
#include "bitops.hpp"
#include "typedefs.hpp"
#include "BitSet.hpp"
#include "BitRank.hpp"

/**
 * BitBoard type: a bit for each chessboard square
 */
class Bb : public BitSet<Bb, Square, u64_t> {
    //declared to catch type cast bugs
    Bb (unsigned int) = delete;
    Bb (int) = delete;

public:
    constexpr Bb () : BitSet() {}
    constexpr explicit Bb (_t bb) : BitSet{bb} {}

    constexpr explicit Bb (long long bb) : BitSet{static_cast<_t>(bb)} {}
    constexpr operator long long () const { return static_cast<long long>(v); }

    constexpr Bb (Square sq) : BitSet(sq) {}
    constexpr Bb (Square::_t sq) : BitSet(sq) {}

    constexpr explicit Bb (File::_t f) : Bb{ULL(0x0101010101010101) << f} {}
    constexpr explicit Bb (Rank::_t r) : Bb{ULL(0xff) << 8*r} {}

    //bidirectional signed rank shift
    constexpr Bb (_t bb, signed r) : Bb{ r >= 0 ? (bb << 8*r) : (bb >> -8*r) } {}

    constexpr Bb operator ~ () const { return Bb{::byteswap(v)}; }

    void move(Square from, Square to) { assert (from != to); *this -= from; *this += to; }

    constexpr BitRank operator[] (Rank r) const { return BitRank{small_cast<BitRank::_t>(v >> 8*r)}; }

    constexpr friend Bb operator << (Bb bb, unsigned offset) { return Bb{static_cast<_t>(bb) << offset}; }
    constexpr friend Bb operator >> (Bb bb, unsigned offset) { return Bb{static_cast<_t>(bb) >> offset}; }

    friend io::ostream& operator << (io::ostream& out, Bb bb) {
        FOR_EACH(Rank, rank) {
            FOR_EACH(File, file) {
                if (bb.has(Square{file, rank})) {
                    out << file;
                }
                else {
                    out << '.';
                }
            }
            out << '\n';
        }
        return out;
    }

};

constexpr Bb Square::rank() const { return Bb{Rank(*this)} - *this; }
constexpr Bb Square::file() const { return Bb{File(*this)} - *this; }
constexpr Bb Square::diagonal() const { return Bb{ULL(0x0102040810204080), Rank(*this) + +File(*this) - 7} - *this; }
constexpr Bb Square::antidiag() const { return Bb{ULL(0x8040201008040201), Rank(*this) - +File(*this)} - *this; }

constexpr Bb Square::line(Direction dir) const {
    switch (dir) {
        case FileDir:     return file();
        case RankDir:     return rank();
        case DiagonalDir: return diagonal();
        case AntidiagDir: return antidiag();
        default: return {};
    }
}

// https://www.chessprogramming.org/0x88
constexpr Bb Square::operator() (signed df, signed dr) const {
    auto sq0x88 = v + (v & 070) + df + 16*dr;

    if (sq0x88 & 0x88) {
        return {}; //out of chess board
    }

    return Bb{ static_cast<square_t>((sq0x88 + (sq0x88 & 7)) >> 1) };
}

#endif
