#ifndef SQUARE_HPP
#define SQUARE_HPP

#include "io.hpp"
#include "Index.hpp"

enum square_t {
    A8, B8, C8, D8, E8, F8, G8, H8,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A1, B1, C1, D1, E1, F1, G1, H1,
};

class Bb;
struct Square : Index<64, square_t, Square> {
    enum { RankShift = 3, RankMask = (Rank::Mask << RankShift) };

    using Index::Index;
    constexpr Square () = default;
    constexpr Square (File file, Rank rank) : Index{static_cast<_t>(file + (rank << RankShift))} {}

    constexpr explicit operator File() const { return static_cast<File::_t>(this->v & File::Mask); }
    constexpr explicit operator Rank() const { return static_cast<Rank::_t>(static_cast<unsigned>(this->v) >> RankShift); }

    /// flip side of the board
    Square& flip() { this->v = static_cast<_t>(this->v ^ RankMask); return *this; }
    constexpr Square operator ~ () const { return static_cast<_t>(this->v ^ RankMask); }

    /// move pawn forward
    constexpr Square rankForward() const { return static_cast<_t>(this->v + A8 - A7); }

    constexpr bool on(Rank rank) const { return Rank{*this} == rank; }
    constexpr bool on(File file) const { return File{*this} == file; }

    // defined in Bb.hpp
    constexpr Bb horizont() const;
    constexpr Bb vertical() const;
    constexpr Bb diagonal() const;
    constexpr Bb antidiag() const;
    constexpr Bb operator() (signed fileOffset, signed rankOffset) const;

    // https://www.chessprogramming.org/0x88
    constexpr bool isValidOffset(signed f, signed r) const {
        return ((static_cast<signed>(this->v + (this->v & 070)) + f + 16*r) & 0x88) == 0;
    }

    constexpr Square offset(signed f, signed r) const {
        assert (isValidOffset(f, r));
        return static_cast<_t>(static_cast<signed>(this->v) + f + (r << RankShift));
    }

    friend io::ostream& operator << (io::ostream& out, Square sq) { return out << File(sq) << Rank(sq); }

    friend io::istream& read(io::istream& in, Square& sq) {
        File file; Rank rank;

        auto before = in.tellg();
        if (!read(in, file) || !read(in, rank)) { return io::fail_pos(in, before); }

        sq = Square{file, rank};
        return in;
    }

};

#endif
