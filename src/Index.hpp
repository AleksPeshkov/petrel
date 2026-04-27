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
    using T = self_type;
    using Arg = T;

public:
    using _t = value_type; // _t v_
    static constexpr int Size = _Size;
    static_assert(Size > 0);
    static constexpr _t Mask = static_cast<_t>(Size-1);
    static constexpr _t Last = static_cast<_t>(Size-1);

    template <typename T>
    using arrayOf = indexed_array<T, Index>;

protected:
    _t v_; // _t v_

public:
    constexpr Index () : v_{} {}
    constexpr explicit Index (_t i) : v_{i} {
        // disabled to make constexpr ::inBetween compile
        /*assertOk();*/
    }

    constexpr _t v() const { /*assertOk();*/ return v_; } // _t v() const { return v_; }

    constexpr void assertOk() const { assert (isOk()); }
    constexpr bool isOk() const { return static_cast<unsigned>(v_) < static_cast<unsigned>(Size); }

    constexpr bool is(_t i) const { return v_ == i; }
    constexpr bool is(T i) const { return v_ == i.v(); }

    constexpr T& operator ++ () { assertOk(); v_ = static_cast<_t>(v_+1); return static_cast<T&>(*this); }
    [[nodiscard]] constexpr T operator ++ (int) { T before{v_}; ++(*this); return before; }

    constexpr T& flip() { assertOk(); v_ = static_cast<_t>(v_ ^ Mask); return static_cast<T&>(*this); }
    constexpr T operator ~ () const { return T{v_}.flip(); }

    friend constexpr bool operator == (Arg a, Arg b) { return a.v() == b.v(); }
    friend constexpr bool operator <  (Arg a, Arg b) { return a.v() < b.v(); }
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
    using T = self_type;

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
    constexpr explicit IndexChar (_t i) : Base{i} {}
    using Base::assertOk;

    constexpr io::char_type to_char() const { return The_string[v_]; }
    friend ostream& operator << (ostream& os, T index) { return os << index.to_char(); }

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

    friend istream& operator >> (istream& is, T& index) {
        io::char_type c{};
        if (is.get(c)) {
            if (!index.from_char(c)) { io::fail_char(is); }
        }
        return is;
    }
};
#define STRUCT_INDEX_CHAR(self_type, Size, value_type) struct self_type : ::IndexChar<self_type, Size, value_type> { using ::IndexChar<self_type, Size, value_type>::IndexChar; }

using node_count_t = u64_t;
enum : node_count_t {
    NodeCountNone = std::numeric_limits<node_count_t>::max(),
    NodeCountMax  = NodeCountNone - 1
};

enum file_t { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, };
struct File : Index<File, 8, file_t> {
    using Index::Index;

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('a' + v_); }
    friend ostream& operator << (ostream& os, File file) { return os << file.to_char(); }

    bool from_char(io::char_type c) {
        if (!('a' <= c && c <= 'h')) { return false; }
        File file{ static_cast<File::_t>(c - 'a') };
        v_ = file.v();
        return true;
    }

    friend istream& operator >> (istream& is, File& file) {
        io::char_type c{};
        if (is.get(c)) {
            if (!file.from_char(c)) { io::fail_char(is); }
        }
        return is;
    }
};

enum rank_t { Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1, };
struct Rank : Index<Rank, 8, rank_t> {
    enum { Shift = 3 };

    using Index::Index;

    constexpr Rank forward() const { return Rank{static_cast<Rank::_t>(v_ + Rank2 - Rank1)}; }

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('8' - v_); }
    friend ostream& operator << (ostream& os, Rank rank) { return os << rank.to_char(); }

    bool from_char(io::char_type c) {
        if (!('1' <= c && c <= '8')) { return false; }
        Rank rank{ static_cast<Rank::_t>('8' - c) };
        v_ = rank.v();
        return true;
    }

    friend istream& operator >> (istream& is, Rank& rank) {
        io::char_type c{};
        if (is.get(c)) {
            if (!rank.from_char(c)) { io::fail_char(is); }
        }
        return is;
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
    static constexpr _t RankMask{Rank::Mask << Rank::Shift};
public:
    static constexpr int Bits{6};
    static constexpr _t None{0xff};

    constexpr Square () : Index{None} {}
    constexpr explicit Square (_t sq) : Index{sq} {}
    constexpr Square (File::_t file, Rank::_t rank) : Square{static_cast<_t>(file + (rank << Rank::Shift))} {}
    constexpr Square (File file, Rank rank) : Square{file.v(), rank.v()} {}
    constexpr Square (File file, Rank::_t rank) : Square{file.v(), rank} {}

    constexpr explicit operator File() const { return File{static_cast<File::_t>(static_cast<int>(v_) & File::Mask)}; }
    constexpr explicit operator Rank() const { return Rank{static_cast<Rank::_t>(static_cast<unsigned>(v_) >> Rank::Shift)}; }

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

    friend ostream& operator << (ostream& os, Square sq) { return os << File{sq} << Rank{sq}; }
};

enum color_t { White, Black };
constexpr color_t operator ~ (color_t color) { return static_cast<color_t>(color ^ 1); }

template <> struct CharMap<color_t> { static constexpr io::czstring The_string = "wb"; };
class Color : public IndexChar<Color, 2, color_t> {
public:
    using IndexChar::IndexChar;
    constexpr Color operator ~ () const { return Color{~v_}; }
};

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

enum class CanBeKiller { No, Yes }; // No = 0, Yes = 1

enum history_type_t { HistoryPawn, HistoryRB, HistoryQN, HistoryKing };
struct HistoryType; STRUCT_INDEX_ENUM (HistoryType, 4, history_type_t);

// 13 bits
struct TtMove {
public:
    using _t = u16_t;
    static constexpr _t None{0}; // null move
    static constexpr int Bits{13};
    static constexpr _t Mask{::singleton<_t>(Bits)-1};

private:
    enum { To = 0, From = To + Square::Bits, Killer = From + Square::Bits };

#ifndef NDEBUG
    union {
        _t v_;
        struct __attribute__((packed)) {
            Square::_t to_:6;
            Square::_t from_:6;
            CanBeKiller canBeKiller_:1;
        } u;
    };
#else
    _t v_;
#endif

public:
    constexpr TtMove () : v_{None} {} // null move
    constexpr explicit TtMove (int n) : v_{static_cast<_t>(n & Mask)} { assertOk(); }

    constexpr TtMove (Square _from, Square _to, CanBeKiller _canBeKiller)
        : v_ {static_cast<_t>(
            (_to.v() << To) | (_from.v() << From)
            | ((_canBeKiller == CanBeKiller::Yes && (_from.v() != 0 || _to.v() != 0)) << Killer)
        )}
    { assertOk(); }

    constexpr void assertOk() const { assert (v_ == None || from().v() != 0 || to().v() != 0); } // check for canonical null move

    constexpr _t v() const { return v_; }
    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

    constexpr Square from() const { return Square{static_cast<Square::_t>(v_ >> From & Square::Mask)}; }
    constexpr Square to() const { return Square{static_cast<Square::_t>(v_ >> To & Square::Mask)}; }
    constexpr CanBeKiller canBeKiller() const { return CanBeKiller{v_ >> Killer & 1}; }

    friend constexpr bool operator == (TtMove a, TtMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(TtMove) == sizeof(u16_t));

// HistoryMove { Square to:6; Square from:6; CanBeKiller:1; HistoryType:2; MoveType:1 } (15 bits)
// Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
// moveType == Special for castling moves and for any pawn move
// Castling encoded as the castling rook moves over own king source square.
// Pawn promotion piece type encoded in place of destination square rank.
// En passant capture encoded as the pawn moves over captured pawn square.
// Null move is encoded as 0 {A8A8}
class HistoryMove {
public:
    using _t = u16_t;
    static constexpr _t None{0}; // null move
    static constexpr int Bits{15};
    static constexpr _t Mask{::singleton<_t>(Bits)-1};

private:
    enum { To = 0, From = To + Square::Bits, Killer = From + Square::Bits, HistType = Killer + 1 };

#ifndef NDEBUG
    union {
        _t v_;
        struct __attribute__((packed)) {
            Square::_t to_:6;
            Square::_t from_:6;
            CanBeKiller canBeKiller_:1;
            history_type_t historyType_:2;
        } u;
    };
#else
    _t v_;
#endif

public:
    constexpr HistoryMove () : v_{None} {} // null move

    constexpr HistoryMove (TtMove ttMove, HistoryType historyType)
        : v_{static_cast<_t>(ttMove.v() | (historyType.v() << HistType))}
    {}

    constexpr HistoryMove (Square from, Square to, CanBeKiller canBeKiller, HistoryType historyType)
        : HistoryMove{TtMove{from, to, canBeKiller}, historyType}
    {}

    constexpr explicit operator TtMove () const { return TtMove{v_}; }

    constexpr void assertOk() const { assert (v_ == None || from().v() != 0 || to().v() != 0); } // check for canonical null move
    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

    constexpr Square from() const { return Square{static_cast<Square::_t>(v_ >> From & Square::Mask)}; }
    constexpr Square to() const { return Square{static_cast<Square::_t>(v_ >> To & Square::Mask)}; }
    constexpr CanBeKiller canBeKiller() const { return CanBeKiller{v_ >> Killer & 1}; }
    constexpr HistoryType historyType() const { return HistoryType{static_cast<HistoryType::_t>(v_ >> HistType & HistoryType::Mask)}; }

    friend constexpr bool operator == (HistoryMove a, HistoryMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(HistoryMove) == sizeof(u16_t));

class Z {
public:
    using _t = u64_t;

    enum zobrist_index_t { Castling = 6, EnPassant = 7 };
    struct Index : ::Index<Index, 8> {
        using Base = ::Index<Index, 8>;
        constexpr Index (PieceType::_t ty) : Base{ty} {}
        constexpr Index (zobrist_index_t ty) : Base{ty} {}
        constexpr Index (PieceType ty) : Base{ty.v()} {}
        constexpr Index (NonKingType ty) : Base{ty.v()} {}
        constexpr Index (PromoType ty) : Base{ty.v()} {}
    };

private:
    _t v_;

    //hand picked set of de Bruijn numbers
    enum : _t {
        ZQueen  = U64(0x0218'a392'cd5d'3dbf),
        ZRook   = U64(0x0245'30de'cb9f'8ead),
        ZBishop = U64(0x02b9'1efc'4b53'a1b3),
        ZKnight = U64(0x02dc'61d5'ecfc'9a51),
        ZPawn   = U64(0x031f'af09'dcda'2ca9),
        ZKing   = U64(0x0352'138a'fdd1'e65b),
        ZCastling = ZRook ^ ZPawn, // rook with castling right encoded as pawn on rank1
        ZEnPassant = ::rotateleft(ZPawn, A4), // A4 => A8, en passant pawn encoded as pawn on rank8
        // ZExtra  = U64(0x03ac'4dfb'4854'6797), // reserved
    };

    static constexpr _t zKey[] = {
        ZQueen, ZRook, ZBishop, ZKnight, ZPawn, ZKing, ZCastling, ZEnPassant
    };

protected:
    // only for testing
    constexpr explicit Z(_t n) : v_{n} {}

public:
    constexpr Z () : v_{0} {}
    constexpr Z(Index ty, Square sq) : v_{::rotateleft(zKey[ty.v()], sq.v())} {}
    constexpr _t v() const { return v_; }

    constexpr Z operator ~ () const { return Z{::byteswap(v_)}; }
    friend constexpr Z operator ^ (Z a, Z b) { return Z{a.v_ ^ b.v_}; }
    [[nodiscard]] friend constexpr bool operator == (Z a, Z b) { return a.v_ == b.v_; }

    friend ostream& operator << (ostream& os, Z z) {
        auto flags = os.flags();

        os << " [" << std::hex << std::setw(16) << std::setfill('0') << "]";
        os << z.v_;

        os.flags(flags);
        return os;
    }
};

static_assert (Z{Z::EnPassant, Square{A4}} == Z{Pawn, Square{A8}});
static_assert (Z{Z::EnPassant, Square{B4}} == Z{Pawn, Square{B8}});

#endif
