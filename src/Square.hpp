#ifndef SQUARE_HPP
#define SQUARE_HPP

#include "io.hpp"
#include "Index.hpp"

enum direction_t { FileDir, RankDir, DiagonalDir, AntidiagDir };
using Direction = Index<4, direction_t>;

enum file_t { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, };

class File : public Index<8, file_t> {
public:
    using Index::Index;

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('a' + v); }
    friend ostream& operator << (ostream& out, File file) { return out << file.to_char(); }

    bool from_char(io::char_type c) {
        File file{ static_cast<File::_t>(c - 'a') };
        if (!file.isOk()) { return false; }
        v = file;
        return true;
    }

    friend istream& operator >> (istream& in, File& file) {
        io::char_type c;
        if (in.get(c)) {
            if (!file.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }

};

enum rank_t { Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1, };
class Rank : public Index<8, rank_t> {
public:
    using Index::Index;
    constexpr Rank forward() const { return Rank{static_cast<Rank::_t>(v + Rank2 - Rank1)}; }

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('8' - v); }
    friend ostream& operator << (ostream& out, Rank rank) { return out << rank.to_char(); }

    bool from_char(io::char_type c) {
        Rank rank{ static_cast<Rank::_t>('8' - c) };
        if (!rank.isOk()) { return false; }
        v = rank;
        return true;
    }

    friend istream& operator >> (istream& in, Rank& rank) {
        io::char_type c;
        if (in.get(c)) {
            if (!rank.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }

};

enum square_t : u8_t {
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
struct Square : Index<64, square_t> {
    enum { RankShift = 3, RankMask = (Rank::Mask << RankShift) };

    using Index::Index;

protected:
    using Index::v;

public:
    constexpr Square (File::_t file, Rank::_t rank) : Index{static_cast<_t>(file + (rank << RankShift))} {}

    constexpr explicit operator File() const { return File{static_cast<File::_t>(v & static_cast<_t>(File::Mask))}; }
    constexpr explicit operator Rank() const { return Rank{static_cast<Rank::_t>(static_cast<unsigned>(v) >> RankShift)}; }

    /// flip side of the board
    Square& flip() { v = static_cast<_t>(static_cast<unsigned>(v) ^ RankMask); return *this; }
    constexpr Square operator ~ () const { return Square{static_cast<_t>(v ^ static_cast<_t>(RankMask))}; }

    /// move pawn forward
    constexpr Square rankForward() const { return Square{static_cast<_t>(v + A8 - A7)}; }

    constexpr bool on(Rank::_t rank) const { return Rank{*this} == rank; }
    constexpr bool on(File::_t file) const { return File{*this} == file; }

    // defined in Bb.hpp
    constexpr Bb rank() const;
    constexpr Bb file() const;
    constexpr Bb diagonal() const;
    constexpr Bb antidiag() const;
    constexpr Bb line(Direction) const;

    constexpr Bb operator() (signed fileOffset, signed rankOffset) const;

    friend ostream& operator << (ostream& out, Square sq) { return out << File{sq} << Rank{sq}; }

    friend istream& operator >> (istream& in, Square& sq) {
        auto before = in.tellg();

        File file; Rank rank;
        in >> file >> rank;

        if (!in) { return io::fail_pos(in, before); }

        sq = Square{file, rank};
        return in;
    }

};

#endif
