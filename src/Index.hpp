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
        i.assertOk();
        return Base::operator[](i.v());
    }

    constexpr const T& operator[](Index i) const {
        i.assertOk();
        return Base::operator[](i.v());
    }
};

//typesafe implementation using "curiously recurring template pattern"
template <class self_type, int _Size, typename value_type = int>
class Index {
    using Self = self_type;
#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)
    using Arg = Self;

public:
    using _t = value_type; // _t v_
    constexpr static int Size = _Size;
    static_assert(Size > 0);
    constexpr static _t Mask = static_cast<_t>(Size-1);
    constexpr static _t Last = static_cast<_t>(Size-1);

    template <typename T>
    using arrayOf = indexed_array<T, Index>;

protected:
    _t v_; // _t v_

public:
    constexpr Index () : v_{} {}
    explicit constexpr Index (_t i) : v_{i} {
        // disabled to make constexpr ::inBetween compile
        /*assertOk();*/
    }

    constexpr _t v() const { assertOk(); return v_; } // _t v() const { return v_; }

    constexpr void assertOk() const { assert (isOk()); }
    [[nodiscard]] constexpr bool isOk() const { return static_cast<unsigned>(v_) < static_cast<unsigned>(Size); }

    constexpr bool is(_t i) const { return v_ == i; }
    constexpr bool is(Index i) const { return v_ == i.v_; }

    constexpr Self& operator ++ () { assertOk(); v_ = static_cast<_t>(v_+1); return SELF; }
    [[nodiscard]] constexpr Self operator ++ (int) { Self before{v_}; ++(*this); return before; }

    constexpr Self& flip() { assertOk(); v_ = static_cast<_t>(v_ ^ Mask); return SELF; }
    constexpr Self operator ~ () const { return Self{v_}.flip(); }

    constexpr friend bool operator == (Arg a, Arg b) { return a.v_ == b.v_; }
    constexpr friend bool operator != (Arg a, Arg b) { return !(a == b); }
    constexpr friend bool operator <  (Arg a, Arg b) { return a.v_ < b.v_; }
    constexpr friend bool operator >  (Arg a, Arg b) { return b < a; }
    constexpr friend bool operator <= (Arg a, Arg b) { return !(a > b); }
    constexpr friend bool operator >= (Arg a, Arg b) { return !(a < b); }

#undef SELF
#undef CONST_SELF
};
#define STRUCT_INDEX(self_type, Size) struct self_type : ::Index<self_type, Size> { using ::Index<self_type, Size>::Index; }
#define STRUCT_INDEX_ENUM(self_type, Size, value_type) struct self_type : ::Index<self_type, Size, value_type> { using ::Index<self_type, Size, value_type>::Index; }

template <typename Enum>
struct CharMap {
    static constexpr io::czstring The_string = nullptr;
};

// Index with character I/O
template <class self_type, int _Size, typename value_type = int>
class IndexChar : public Index<self_type, _Size, value_type> {
    using Base = Index<self_type, _Size, value_type>;
    friend class Index<self_type, _Size, value_type>;
    using Self = self_type;

    static constexpr io::czstring The_string = CharMap<value_type>::The_string;

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

protected:
    using Base::v_;

public:
    using typename Base::_t;
    constexpr IndexChar () : Base{} {}
    explicit constexpr IndexChar (_t i) : Base{i} {}
    using Base::assertOk;

    constexpr io::char_type to_char() const { return The_string[v_]; }
    friend ostream& operator << (ostream& out, Self index) { return out << index.to_char(); }

    constexpr bool from_char(io::char_type c) {
        const auto* begin = The_string;
        const auto* end = begin + _Size;
        const auto* p = std::find(begin, end, c);
        if (p == end) return false;
        v_ = static_cast<_t>(p - begin);
        assertOk();
        assert (c == to_char());
        return true;
    }

    friend istream& operator >> (istream& in, Self& index) {
        io::char_type c;
        if (in.get(c)) {
            if (!index.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }
};
#define STRUCT_INDEX_CHAR(self_type, Size, value_type) struct self_type : ::IndexChar<self_type, Size, value_type> { using ::IndexChar<self_type, Size, value_type>::IndexChar; }

// search tree distance in halfmoves
struct Ply : Index<Ply, 64> {
    explicit constexpr Ply(int i) : Index{i > 0 ? i : 0} { assertOk(); }
    friend constexpr Ply operator""_ply(unsigned long long);
    friend Ply operator + (Ply a, Ply b) { return Ply{a.v() + b.v()}; }
    friend Ply operator - (Ply a, Ply b) { return Ply{a.v() - b.v()}; }
    friend Ply operator * (Ply a, int n) { return Ply{a.v() * n}; }
    friend Ply operator / (Ply a, int n) { return Ply{a.v() / n}; }

    friend ostream& operator << (ostream& out, Ply ply) { return out << ply.v(); }

    friend istream& operator >> (istream& in, Ply& ply) {
        int n;
        auto before = in.tellg();
        in >> n;
        if (!(0 <= n && n <= Last)) { return io::fail_pos(in, before); }
        ply = Ply{n};
        return in;
    }
};
constexpr Ply MaxPly{Ply::Last}; // Ply is limited to [0 .. MaxPly]
constexpr Ply operator""_ply(unsigned long long n) { return Ply{static_cast<Ply::_t>(n)}; }

using node_count_t = u64_t;
enum : node_count_t {
    NodeCountNone = std::numeric_limits<node_count_t>::max(),
    NodeCountMax  = NodeCountNone - 1
};

enum file_t { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, };
struct File : Index<File, 8, file_t> {
    using Index::Index;

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('a' + v_); }
    friend ostream& operator << (ostream& out, File file) { return out << file.to_char(); }

    bool from_char(io::char_type c) {
        File file{ static_cast<File::_t>(c - 'a') };
        if (!file.isOk()) { return false; }
        v_ = file.v();
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
struct Rank : Index<Rank, 8, rank_t> {
    using Index::Index;

    constexpr Rank forward() const { return Rank{static_cast<Rank::_t>(v_ + Rank2 - Rank1)}; }

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('8' - v_); }
    friend ostream& operator << (ostream& out, Rank rank) { return out << rank.to_char(); }

    bool from_char(io::char_type c) {
        Rank rank{ static_cast<Rank::_t>('8' - c) };
        if (!rank.isOk()) { return false; }
        v_ = rank.v();
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
struct Direction; STRUCT_INDEX_ENUM (Direction, 4, direction_t);

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
class Square : public Index<Square, 64, square_t> {
    using Base = Index<Square, 64, square_t>;
    friend class Index<Square, 64, square_t>;

protected:
    enum { RankShift = 3 };
    static constexpr _t RankMask{Rank::Mask << RankShift};
    using Index::v_;

public:
    constexpr static _t None{0xff};

    constexpr Square () : Base{None} {}
    constexpr explicit Square (_t sq) : Base{sq} {}
    constexpr Square (File::_t file, Rank::_t rank) : Square{static_cast<_t>(file + (rank << RankShift))} {}
    constexpr Square (File file, Rank rank) : Square{file.v(), rank.v()} {}
    constexpr Square (File file, Rank::_t rank) : Square{file.v(), rank} {}

    constexpr explicit operator File() const { return File{static_cast<File::_t>(static_cast<int>(v_) & File::Mask)}; }
    constexpr explicit operator Rank() const { return Rank{static_cast<Rank::_t>(static_cast<unsigned>(v_) >> RankShift)}; }

    /// flip side of the board
    constexpr Square& flip() { v_ = static_cast<_t>(v_ ^ RankMask); return *this; }
    constexpr Square operator ~ () const { return Square{v_}.flip(); }

    /// move pawn forward
    constexpr Square rankForward() const { return Square{static_cast<_t>(v_ + A8 - A7)}; }

    constexpr bool on(Rank::_t rank) const { return Rank{*this} == Rank{rank}; }
    constexpr bool on(File::_t file) const { return File{*this} == File{file}; }

    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

// defined in Bb.hpp

    constexpr Bb bb(signed fileOffset, signed rankOffset) const; // BitBoard of the square + offset (or empty if not on board)
    constexpr Bb bbRank() const; // BitBoard of the rank of the square (excluding the square itself)
    constexpr Bb bbFile() const; // BitBoard of the file of the square (excluding the square itself)
    constexpr Bb bbDiagonal() const; // BitBoard of the diagonal of the square (excluding the square itself)
    constexpr Bb bbAntidiag() const; // BitBoard of the antidiagonal of the square (excluding the square itself)
    constexpr Bb bbDirection(Direction) const; // BitBoard of the direction of the square (excluding the square itself)

    friend ostream& operator << (ostream& out, Square sq) { return out << File{sq} << Rank{sq}; }
};

enum color_t { White, Black };
constexpr color_t operator ~ (color_t color) { return static_cast<color_t>(color ^ 1); }

template <> struct CharMap<color_t> { static constexpr io::czstring The_string = "wb"; };
class Color : public IndexChar<Color, 2, color_t> {
    using Base = IndexChar<Color, 2, color_t>;
    using Base::v_;
public:
    using Base::Base;
    constexpr Color operator ~ () const { return Color{~v_}; }
};

// color to move of the given ply
constexpr Color::_t distance(Color c, Ply ply) { return static_cast<Color::_t>((ply.v() ^ static_cast<unsigned>(c.v())) & Color::Mask); }

enum side_to_move_t {
    My, // side to move
    Op, // not side to move
};
constexpr side_to_move_t operator ~ (side_to_move_t si) { return static_cast<side_to_move_t>(si ^ 1); }
struct Side; STRUCT_INDEX_ENUM (Side, 2, side_to_move_t);

enum chess_variant_t { Orthodox, Chess960 };
struct ChessVariant; STRUCT_INDEX_ENUM (ChessVariant, 2, chess_variant_t);

enum castling_side_t { KingSide, QueenSide };
template <> struct CharMap<castling_side_t> { static constexpr io::czstring The_string = "kq"; };
struct CastlingSide; STRUCT_INDEX_CHAR (CastlingSide, 2, castling_side_t);

enum piece_index_t : u8_t { TheKing }; // king index is always 0
struct Pi; STRUCT_INDEX_ENUM (Pi, 16, piece_index_t);

enum piece_type_t {
    Queen = 0,
    Rook = 1,
    Bishop = 2,
    Knight = 3,
    Pawn = 4,
    King = 5,
};
template <> struct CharMap<piece_type_t> { static constexpr io::czstring The_string = "qrbnpk"; };

// Queen, Rook, Bishop
struct SliderType; STRUCT_INDEX_ENUM (SliderType, 3, piece_type_t);

// Queen, Rook, Bishop, Knight
struct PromoType; STRUCT_INDEX_CHAR (PromoType, 4, piece_type_t);

 // Queen, Rook, Bishop, Knight, Pawn
struct NonKingType; STRUCT_INDEX_ENUM (NonKingType, 5, piece_type_t);

// Queen, Rook, Bishop, Knight, Pawn, King
struct PieceType : IndexChar<PieceType, 6, piece_type_t> {
    constexpr PieceType (PieceType::_t ty) : IndexChar{ty} {}
    constexpr PieceType (SliderType ty) : IndexChar{ty.v()} {}
    constexpr PieceType (PromoType ty) : IndexChar{ty.v()} {}
    constexpr PieceType (NonKingType ty) : IndexChar{ty.v()} {}
};

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; } // Queen, Rook, Bishop
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; } // Knight, Pawn, King

// encoding of the promoted piece type inside "to" square
constexpr Rank rankOf(PromoType ty) { return Rank{static_cast<Rank::_t>(ty.v())}; }

// decoding promoted piece type from move destination square rank
constexpr PromoType promoTypeFrom(Rank rank) { return PromoType{static_cast<PromoType::_t>(rank.v())}; }

// continue or stop search
enum class ReturnStatus {
    Continue, // continue search normally
    Stop,     // stop current search (timeout or other termination reason)
    Cutoff,   // prune current node search (futility or beta cutoff)
};

enum history_type_t { HistoryRBN, HistoryQueen, HistoryPawn, HistoryKing };
struct HistoryType; STRUCT_INDEX_ENUM (HistoryType, 4, history_type_t);

constexpr HistoryType historyType(PieceType ty) {
    constexpr HistoryType::_t fromPieceType[] = { HistoryQueen, HistoryRBN, HistoryRBN, HistoryRBN, HistoryPawn, HistoryKing };
    return HistoryType{fromPieceType[ty.v()]};
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
    _t v_;

public:
    static constexpr _t None{0};
    static constexpr int Size = HistoryType::Size * Square::Size * Square::Size;
    struct HistoryIndex; STRUCT_INDEX (HistoryIndex, Size);

    // null move
    constexpr HistoryMove() : v_{None} {}

    constexpr HistoryMove (PieceType ty, Square from, Square to)
        : v_ {static_cast<_t>((::historyType(ty).v() << Shift::Type) + (from.v() << Shift::From) + (to.v() << Shift::To))}
    {}

    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

    constexpr operator HistoryIndex () const { return HistoryIndex{v_}; }

    // moved piece type
    constexpr HistoryType historyType() const { return HistoryType{static_cast<HistoryType::_t>(v_ >> Shift::Type)}; }

    // source square the piece moved from
    constexpr Square from() const { return Square{static_cast<Square::_t>(v_ >> Shift::From & Square::Mask)}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{static_cast<Square::_t>(v_ >> Shift::To & Square::Mask)}; }

    friend constexpr bool operator == (HistoryMove a, HistoryMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(HistoryMove) == sizeof(u16_t));

// Position independent move is 13 bits with the special move type flag to mark either castling, promotion or en passant move
// Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
class UciMove {
    using _t = u16_t;

    enum Shift { To = 0, From = To + 6, Special = From + 6 };

    _t v_;

public:
    static constexpr _t NoMove{0};

    // null move
    constexpr UciMove() : v_{NoMove} {}

    constexpr UciMove (Square from, Square to, bool special)
        : v_ {static_cast<_t>((special << Shift::Special) + (from.v() << Shift::From) + (to.v() << Shift::To))}
    {}

    constexpr bool none() const { return v_ == NoMove; }
    constexpr bool any() const { return !none(); }

    constexpr bool isSpecial() const { return v_ >> Shift::Special & 1; }

    // source square the piece moved from
    constexpr Square from() const { return Square{static_cast<Square::_t>(v_ >> Shift::From & Square::Mask)}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{static_cast<Square::_t>(v_ >> Shift::To & Square::Mask)}; }

    friend constexpr bool operator == (UciMove a, UciMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(UciMove) == sizeof(u16_t));

#endif
