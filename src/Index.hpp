#ifndef INDEX_HPP
#define INDEX_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <concepts>
#include <limits>
#include <ranges>
#include <type_traits>

#include "assert.hpp"
#include "bitops.hpp"
#include "io.hpp"

template <typename T>
concept IndexLike =
    requires { T::Size; }
    && std::is_integral_v<std::remove_cv_t<decltype(T::Size)>>
    && (T::Size > 0)
    && requires { typename T::_t; }
    && std::is_constructible_v<T, typename T::_t>
    && (std::is_integral_v<typename T::_t> || std::is_enum_v<typename T::_t>);

template <IndexLike Index>
static constexpr auto range() {
    return std::views::iota(0, Index::Size) | std::views::transform([](int i) {
        return Index{static_cast<Index::_t>(i)};
    });
}

template <IndexLike Index>
static constexpr auto reverse_range() { return std::views::reverse(range<Index>()); }

template <typename T, IndexLike Index>
class indexed_array : public std::array<T, Index::Size> {
    using Base = std::array<T, Index::Size>;

    // Delete all integral overloads
    template <typename I> requires std::integral<I>
    T& operator[](I) = delete;

    template <typename I> requires std::integral<I>
    const T& operator[](I) const = delete;

public:
    // allow only indexing with Index
    constexpr T& operator[](Index i) {
        return Base::operator[](static_cast<size_t>(static_cast<typename Index::_t>(i)));
    }

    constexpr const T& operator[](Index i) const {
        return Base::operator[](static_cast<size_t>(static_cast<typename Index::_t>(i)));
    }
};

template <int _Size, typename _element_type = int>
class Index {
public:
    using _t = _element_type;
    constexpr static int Size = _Size;
    static_assert(Size > 0);
    constexpr static _t Mask =  static_cast<_t>(Size-1);
    constexpr static _t Last =  static_cast<_t>(Size-1);

    template <typename T>
    using arrayOf = indexed_array<T, Index>;

protected:
    _t v;

public:
    explicit constexpr Index (_t i = static_cast<_t>(0)) : v{i} { assertOk(); }
    constexpr operator const _t& () const { return v; }

    constexpr void assertOk() const { assert (isOk()); }
    [[nodiscard]] constexpr bool isOk() const { return static_cast<unsigned>(v) < static_cast<unsigned>(Size); }

    constexpr bool is(_t i) const { return v == i; }

    constexpr Index& operator ++ () { assertOk(); v = static_cast<_t>(v+1); return *this; }

    constexpr Index& flip() { assertOk(); v = static_cast<_t>(v ^ Mask); return *this; }
    constexpr Index operator ~ () const { return Index{static_cast<_t>(v)}.flip(); }

    constexpr friend bool operator < (Index a, Index b) { return a.v < b.v; }
    constexpr friend bool operator <= (Index a, Index b) { return a.v <= b.v; }

    friend ostream& operator << (ostream& out, Index index) { return out << static_cast<int>(index.v); }

    friend istream& operator >> (istream& in, Index& index) {
        int n;
        auto before = in.tellg();
        in >> n;
        if (n < 0 || Last < n) { return io::fail_pos(in, before); }
        index = Index{static_cast<_t>(n)};
        return in;
    }
};

template <typename Enum>
struct CharMap {
    static constexpr io::czstring The_string = nullptr;
};

// Index with character I/O
template <int _Size, typename _element_type = int>
class IndexChar : public Index<_Size, _element_type> {
    using Base = Index<_Size, _element_type>;
    using Base::v;

    static constexpr io::czstring The_string = CharMap<_element_type>::The_string;

    static_assert(The_string != nullptr, "CharMap<Enum> must be specialized with a valid string");

    static_assert([] {
        // Ensure at least _Size non-null characters
        for (int i = 0; i < _Size; ++i)
            if (The_string[i] == '\0')
                return false;
        return true;
    }(), "CharMap string must have at least _Size non-null characters");

    static_assert([] {
        // Ensure first _Size characters are unique
        for (int i = 0; i < _Size; ++i)
            for (int j = i + 1; j < _Size; ++j)
                if (The_string[i] == The_string[j])
                    return false;
        return true;
    }(), "CharMap string must have unique characters in first _Size positions");

public:
    using typename Base::_t;
    using Base::Base;
    using Base::assertOk;

    constexpr io::char_type to_char() const { return The_string[v]; }
    friend ostream& operator << (ostream& out, IndexChar index) { return out << index.to_char(); }

    constexpr bool from_char(io::char_type c) {
        const auto* begin = The_string;
        const auto* end = begin + _Size;
        const auto* p = std::find(begin, end, c);
        if (p == end) return false;
        v = static_cast<_t>(p - begin);
        assertOk();
        assert (c == to_char());
        return true;
    }

    friend istream& operator >> (istream& in, IndexChar& index) {
        io::char_type c;
        if (in.get(c)) {
            if (!index.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }
};

// search tree distance in halfmoves
class Ply : public Index<64> {
public:
    explicit constexpr Ply(int i) : Index{i > 0 ? i : 0} { assertOk(); }
    friend constexpr Ply operator""_ply(unsigned long long);
    friend auto operator <=> (const Ply& a, const Ply& b) = default;
};
constexpr Ply MaxPly{Ply::Last}; // Ply is limited to [0 .. MaxPly]
constexpr Ply operator""_ply(unsigned long long n) { return Ply{static_cast<Ply::_t>(n)}; }

using node_count_t = u64_t;
enum : node_count_t {
    NodeCountNone = std::numeric_limits<node_count_t>::max(),
    NodeCountMax  = NodeCountNone - 1
};

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

enum direction_t { FileDir, RankDir, DiagonalDir, AntidiagDir };
using Direction = Index<4, direction_t>;

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

enum color_t { White, Black };
template <> struct CharMap<color_t> { static constexpr io::czstring The_string = "wb"; };
using Color = IndexChar<2, color_t>;

// color to move of the given ply
constexpr Color::_t distance(Color c, Ply ply) { return static_cast<Color::_t>((ply ^ static_cast<unsigned>(c)) & Color::Mask); }

enum side_to_move_t {
    My, // side to move
    Op, // not side to move
};
using Side = Index<2, side_to_move_t>;
constexpr Side::_t operator ~ (Side::_t si) {
    return static_cast<Side::_t>(si ^ Side::Mask);
}

enum chess_variant_t { Orthodox, Chess960 };
using ChessVariant = Index<2, chess_variant_t>;

enum castling_side_t { KingSide, QueenSide };
template <> struct CharMap<castling_side_t> { static constexpr io::czstring The_string = "kq"; };
using CastlingSide = IndexChar<2, castling_side_t>;

enum piece_index_t : u8_t { TheKing }; // king index is always 0
using Pi = Index<16, piece_index_t>; //piece index 0..15

enum piece_type_t {
    Queen = 0,
    Rook = 1,
    Bishop = 2,
    Knight = 3,
    Pawn = 4,
    King = 5,
};
template <> struct CharMap<piece_type_t> { static constexpr io::czstring The_string = "qrbnpk"; };

using SliderType = Index<3, piece_type_t>;    // Queen, Rook, Bishop
using PromoType = IndexChar<4, piece_type_t>; // Queen, Rook, Bishop, Knight
using NonKingType = Index<5, piece_type_t>;   // Queen, Rook, Bishop, Knight, Pawn
using PieceType = IndexChar<6, piece_type_t>; // Queen, Rook, Bishop, Knight, Pawn, King

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; } // Queen, Rook, Bishop
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; } // Knight, Pawn, King

// encoding of the promoted piece type inside "to" square
constexpr Rank rankOf(PromoType ty) { return Rank{static_cast<Rank::_t>(static_cast<PromoType::_t>(ty))}; }

// decoding promoted piece type from move destination square rank
constexpr PromoType promoTypeFrom(Rank rank) { return PromoType{static_cast<PromoType::_t>(static_cast<Rank::_t>(rank))}; }

// continue or stop search
enum class ReturnStatus {
    Continue,   // continue search normally
    Stop,       // stop current search (timeout or other termination reason)
    BetaCutoff, // prune current node search
};


enum history_type_t { HistoryRBN, HistoryQueen, HistoryPawn, HistoryKing };

using HistoryType = Index<4, history_type_t>;
constexpr HistoryType::_t historyType(PieceType::_t ty) {
    constexpr HistoryType::_t fromPieceType[] = { HistoryQueen, HistoryRBN, HistoryRBN, HistoryRBN, HistoryPawn, HistoryKing };
    return fromPieceType[ty];
}

constexpr bool operator == (PieceType ty, HistoryType ht) { return ::historyType(ty) == ht; }

// HistoryMove { Square to; Square from; HistoryType } (14 bits)
// Castling encoded as the castling rook moves over own king source square.
// Pawn promotion piece type encoded in place of destination square rank.
// En passant capture encoded as the pawn moves over captured pawn square.
// Null move is encoded as 0 {A8A8}
class HistoryMove {
    enum Shift { To = 0, From = To + 6, Type = From + 6 };

    using _t = u16_t;
    _t v;

public:
    static constexpr int Size = HistoryType::Size * Square::Size * Square::Size;
    using Index = ::Index<Size>;

    // null move
    constexpr HistoryMove() : v{0} {}

    constexpr HistoryMove (PieceType ty, Square from, Square to)
        : v {static_cast<_t>((::historyType(ty) << Shift::Type) + (from << Shift::From) + (to << Shift::To))}
    {}

    constexpr operator bool () const { return v; }

    constexpr operator Index () const { return Index{v}; }

    // moved piece type
    constexpr HistoryType historyType() const { return HistoryType{static_cast<HistoryType::_t>(v >> Shift::Type)}; }

    // source square the piece moved from
    constexpr Square from() const { return Square{static_cast<Square::_t>(v >> Shift::From & Square::Mask)}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{static_cast<Square::_t>(v >> Shift::To & Square::Mask)}; }

    friend constexpr bool operator == (HistoryMove a, HistoryMove b) { return a.v == b.v; }
};

static_assert (sizeof(HistoryMove) == sizeof(u16_t));

// Position independent move is 13 bits with the special move type flag to mark either castling, promotion or en passant move
// Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
class UciMove {
    using _t = u16_t;

    enum Shift { To = 0, From = To + 6, Special = From + 6 };

    _t v;

public:
    // null move
    constexpr UciMove() : v{0} {}

    constexpr UciMove (Square from, Square to, bool special)
        : v {static_cast<_t>((special << Shift::Special) + (from << Shift::From) + (to << Shift::To))}
    {}

    constexpr operator bool () const { return v; }

    constexpr bool isSpecial() const { return v >> Shift::Special & 1; }

    // source square the piece moved from
    constexpr Square from() const { return Square{static_cast<Square::_t>(v >> Shift::From & Square::Mask)}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{static_cast<Square::_t>(v >> Shift::To & Square::Mask)}; }

    friend constexpr bool operator == (UciMove a, UciMove b) { return a.v == b.v; }
};

static_assert (sizeof(UciMove) == sizeof(u16_t));

#endif
