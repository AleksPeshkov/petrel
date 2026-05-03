#ifndef INDEX_HPP
#define INDEX_HPP

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <ranges>

#include "bitops.hpp"
#include "io.hpp"

template <typename T>
concept enum_or_integral = std::is_enum_v<T> || std::is_integral_v<T>;

template <typename Index>
concept IndexLike = requires {
    { Index::size() } -> std::integral; requires Index::size() >= 1;

    typename Index::_t; requires enum_or_integral<typename Index::_t>;
    requires std::is_constructible_v<Index, typename Index::_t>;

    requires requires (Index i) { { +i } -> std::convertible_to<int>; };
};

template <IndexLike Index> constexpr auto range() {
    return std::views::iota(0, Index::size()) | std::views::transform(
        [](int i) { return Index{static_cast<Index::_t>(i)}; }
    );
}

template <int Size> constexpr auto range() { return std::views::iota(0, Size); }

// Multidimensional array template
template <typename V, IndexLike Index, IndexLike... Indices>
class array : public std::array<array<V, Indices...>, Index::size()> {
    using Base = std::array<array<V, Indices...>, Index::size()>;

    // delete all integral overloads
    template <typename N> requires std::integral<N> auto& operator[](N) = delete;
    template <typename N> requires std::integral<N> const auto& operator[](N) const = delete;

public:
    constexpr auto& operator[](Index i) { return Base::operator[](+i); }
    constexpr const auto& operator[](Index i) const { return Base::operator[](+i); }
};

// Partial specialization for single dimension
template <typename V, IndexLike Index>
class array<V, Index> : public std::array<V, Index::size()> {
    using Base = std::array<V, Index::size()>;

    // delete all integral overloads
    template <typename N> requires std::integral<N> auto& operator[](N) = delete;
    template <typename N> requires std::integral<N> const auto& operator[](N) const = delete;

public:
    constexpr auto& operator[](Index i) { return Base::operator[](+i); }
    constexpr const auto& operator[](Index i) const { return Base::operator[](+i); }
};

// Typesafe implementation using "curiously recurring template pattern"
template <class self_type, size_t Size, typename value_type = int>
class Index {
    using Self = self_type;
    using Arg = Self;

public:
    using _t = value_type; // _t v_

protected:
    _t v_; // _t v_

public:
    static constexpr int size() { static_assert (Size >= 1); return Size; }
    static constexpr int bit_width() { return std::bit_width(size() - 1u); }
    static constexpr _t mask() { return static_cast<_t>(::singleton<unsigned>(bit_width()) - 1u); }
    static constexpr _t last() { return static_cast<_t>(size() - 1); }

    static constexpr bool isOk(int n) { return 0 <= n && n < size(); }
    constexpr bool isOk() const { return isOk(v_); }
    constexpr void assertOk() const { assert (isOk()); }

    constexpr Index () : v_{} {}
    constexpr explicit Index (_t i) : v_{i} {} // can be invalid default value

    constexpr int operator + () const { assertOk(); return +v_; } // static_cast<int>
    constexpr _t  operator * () const requires std::is_enum_v<_t> { assertOk(); return v_; } // static_cast<_t>(v_)

    constexpr bool is(_t i) const { return v_ == i; }
    constexpr bool is(Self i) const { return is(*i); }

    template <typename S> constexpr int pack(S shift) { return ::pack(v_, shift); }
    template <typename P, typename S> constexpr P pack(S shift) { return ::pack<P>(v_, shift); }

    template <typename P, typename S> static constexpr Self unpack(P packed, S shift) {
        return Self{::unpack(packed, shift, mask())};
    }

    constexpr Self& operator ++ () { assertOk(); v_ = static_cast<_t>(v_+1); return static_cast<Self&>(*this); }
    [[nodiscard]] constexpr Self operator ++ (int) { Self before{v_}; ++(*this); return before; }

    constexpr Self operator ~ () const { assertOk(); return Self{static_cast<_t>(v_ ^ mask())}; }

    friend constexpr bool operator == (Arg a, Arg b) { return +a == +b; }
    friend constexpr bool operator <  (Arg a, Arg b) { return +a < +b; }
};

template <typename Enum>
struct CharMap {
    static constexpr io::czstring The_string = nullptr;
};

// Index with character I/O
template <class self_type, int _Size, typename value_type = int>
class IndexChar : public Index<self_type, _Size, value_type> {
    using Base = Index<self_type, _Size, value_type>;
    using Self = self_type;

    static constexpr io::czstring The_string = CharMap<value_type>::The_string;

    static_assert (The_string != nullptr, "CharMap<Enum> must be specialized with a valid string");

    static_assert ([] {
        // Ensure at least _Size non-null characters
        for (int i = 0; i < _Size; ++i)
            if (The_string[i] == '\0')
                return false;
        return true;
    }(), "CharMap string must have at least _Size non-null characters");

    static_assert ([] {
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
    friend ostream& operator << (ostream& os, Self index) { return os << index.to_char(); }

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

    friend istream& operator >> (istream& is, Self& index) {
        io::char_type c{};
        if (is.get(c)) {
            if (!index.from_char(c)) { io::fail_char(is); }
        }
        return is;
    }
};

enum file_t { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, };
struct File : Index<File, 8, file_t> {
    using Index::Index;

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('a' + v_); }
    friend ostream& operator << (ostream& os, File file) { return os << file.to_char(); }

    bool from_char(io::char_type c) {
        if (!('a' <= c && c <= 'h')) { return false; }
        File file{ static_cast<File::_t>(c - 'a') };
        v_ = *file;
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
    using Index::Index;

    constexpr Rank forward() const { return Rank{static_cast<Rank::_t>(v_ + Rank2 - Rank1)}; }

    constexpr io::char_type to_char() const { return static_cast<io::char_type>('8' - v_); }
    friend ostream& operator << (ostream& os, Rank rank) { return os << rank.to_char(); }

    bool from_char(io::char_type c) {
        if (!('1' <= c && c <= '8')) { return false; }
        Rank rank{ static_cast<Rank::_t>('8' - c) };
        v_ = *rank;
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
struct Direction : Index<Direction, 4, direction_t> { using Index::Index; };

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
    enum { FileShift = 0, RankShift = FileShift + File::bit_width() };

public:
    static constexpr _t None{0xff};

    constexpr Square () : Index{None} {}
    constexpr explicit Square (_t sq) : Index{sq} {}
    constexpr Square (File file, Rank rank) : Square{static_cast<_t>(file.pack(FileShift) | rank.pack(RankShift))} {}
    constexpr Square (File file, Rank::_t rank) : Square{file, Rank{rank}} {}
    constexpr Square (File::_t file, Rank::_t rank): Square{File{file}, Rank{rank}} {}

    constexpr File file() const { return File::unpack(v_, FileShift); }
    constexpr Rank rank() const { return Rank::unpack(v_, RankShift); }

    // flip side of the board
    constexpr Square operator ~ () const {
        return Square{static_cast<_t>(v_ ^ Rank{Rank::mask()}.pack(RankShift))};
    }

    /// move pawn forward
    constexpr Square rankForward() const { return Square{static_cast<_t>(v_ + A8 - A7)}; }

    constexpr bool on(Rank::_t r) const { return rank() == Rank{r}; }
    constexpr bool on(File::_t f) const { return file() == File{f}; }

    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

// defined in Bb.hpp

    constexpr Bb bb(signed fileOffset, signed rankOffset) const; // BitBoard of the square + offset (or empty if not on board)
    constexpr Bb bbRank() const; // BitBoard of the rank of the square (excluding the square itself)
    constexpr Bb bbFile() const; // BitBoard of the file of the square (excluding the square itself)
    constexpr Bb bbDiagonal() const; // BitBoard of the diagonal of the square (excluding the square itself)
    constexpr Bb bbAntidiag() const; // BitBoard of the antidiagonal of the square (excluding the square itself)
    constexpr Bb bbDirection(Direction) const; // BitBoard of the direction of the square (excluding the square itself)

    friend ostream& operator << (ostream& os, Square sq) { return os << sq.file() << sq.rank(); }
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
struct Side : Index<Side, 2, side_to_move_t> { using Index::Index; };

enum chess_variant_t { Orthodox, Chess960 };
struct ChessVariant : Index<ChessVariant, 2, chess_variant_t> { using Index::Index; };

enum castling_side_t { KingSide, QueenSide };
template <> struct CharMap<castling_side_t> { static constexpr io::czstring The_string = "kq"; };
struct CastlingSide : IndexChar<CastlingSide, 2, castling_side_t> { using IndexChar::IndexChar; };

enum piece_index_t : u8_t { TheKing }; // king index is always 0
struct Pi : Index<Pi, 16, piece_index_t> { using Index::Index; };

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
struct SliderType : Index<SliderType, 3, piece_type_t> { using Index::Index; };

// Queen, Rook, Bishop, Knight
struct PromoType : IndexChar<PromoType, 4, piece_type_t> { using IndexChar::IndexChar; };

 // Queen, Rook, Bishop, Knight, Pawn
struct NonKingType : Index<NonKingType, 5, piece_type_t> { using Index::Index; };

// Queen, Rook, Bishop, Knight, Pawn, King
struct PieceType : IndexChar<PieceType, 6, piece_type_t> {
    constexpr PieceType (PieceType::_t ty) : IndexChar{ty} {}
    constexpr PieceType (SliderType ty) : IndexChar{*ty} {}
    constexpr PieceType (PromoType ty) : IndexChar{*ty} {}
    constexpr PieceType (NonKingType ty) : IndexChar{*ty} {}
};

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; } // Queen, Rook, Bishop
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; } // Knight, Pawn, King

// encoding of the promoted piece type inside "to" square
constexpr Rank rankOf(PromoType ty) { return Rank{static_cast<Rank::_t>(*ty)}; }

// decoding promoted piece type from move destination square rank
constexpr PromoType promoTypeFrom(Rank rank) { return PromoType{static_cast<PromoType::_t>(*rank)}; }

// continue or stop search
enum class ReturnStatus {
    Continue, // continue search normally
    Stop,     // stop current search (timeout or other termination reason)
    Cutoff,   // prune current node search (futility or beta cutoff)
};

enum history_type_t { HistorySpecial, HistoryRB, HistoryQN, HistoryKing };
struct HistoryType : Index<HistoryType, 4, history_type_t> { using Index::Index; };

// HistoryMove { Square to; Square from; HistoryType } (14 bits)
// Castling encoded as the castling rook moves over own king source square.
// Pawn promotion piece type encoded in place of destination square rank.
// En passant capture encoded as the pawn moves over captured pawn square.
// Null move is encoded as 0 {A8A8}
class HistoryMove {
    using _t = u16_t;

    enum { ShiftTo = 0, ShiftFrom = ShiftTo + Square::bit_width(), ShiftType = ShiftFrom + Square::bit_width()};
    static constexpr _t None{0};

    _t v_;

public:
    static constexpr int Size = HistoryType::size() * Square::size() * Square::size();
    struct HistoryIndex : Index<HistoryIndex, Size> { using Index::Index; };

    constexpr HistoryMove() : v_{None} {} // null move
    constexpr HistoryMove (Square from, Square to, HistoryType historyType)
        : v_ {static_cast<_t>(from.pack(ShiftFrom) | to.pack(ShiftTo) | historyType.pack(ShiftType))}
    { assert (v_ == None || +from != 0 || +to != 0); } // check for canonical null move

    constexpr bool none() const { return v_ == None; }
    constexpr bool any() const { return !none(); }

    constexpr operator HistoryIndex () const { return HistoryIndex{v_}; }

    constexpr Square from() const { return Square::unpack(v_, ShiftFrom); }
    constexpr Square to() const { return Square::unpack(v_, ShiftTo); }
    constexpr HistoryType historyType() const { assert (any()); return HistoryType::unpack(v_, ShiftType); }

    friend constexpr bool operator == (HistoryMove a, HistoryMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(HistoryMove) == sizeof(u16_t));

// Position independent move is 13 bits with the special move type flag to mark either castling, promotion or en passant move
// Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
class UciMove {
    using _t = u16_t;

    enum { ShiftTo = 0, ShiftFrom = ShiftTo + Square::bit_width(), ShiftSpecial = ShiftFrom + Square::bit_width()};
    static constexpr _t NoMove{0};

    _t v_;

public:
    constexpr UciMove() : v_{NoMove} {} // null move
    constexpr UciMove (Square from, Square to, bool special)
        : v_ {static_cast<_t>(from.pack(ShiftFrom) | to.pack(ShiftTo) | ::pack(special, ShiftSpecial))}
    {}

    constexpr bool none() const { return v_ == NoMove; }
    constexpr bool any() const { return !none(); }

    constexpr Square from() const { return Square::unpack(v_, ShiftFrom); }
    constexpr Square to() const { return Square::unpack(v_, ShiftTo); }
    constexpr bool isSpecial() const { return ::unpack(v_, ShiftSpecial, true); }

    friend constexpr bool operator == (UciMove a, UciMove b) { return a.v_ == b.v_; }
};

static_assert (sizeof(UciMove) == sizeof(u16_t));

class Z {
public:
    using _t = u64_t;

    enum zobrist_index_t { Castling = 6, EnPassant = 7 };
    struct Index : ::Index<Index, 8> {
        using Base = ::Index<Index, 8>;
        constexpr Index (PieceType::_t ty) : Base{ty} {}
        constexpr Index (zobrist_index_t ty) : Base{ty} {}
        constexpr Index (PieceType ty) : Base{*ty} {}
        constexpr Index (NonKingType ty) : Base{*ty} {}
        constexpr Index (PromoType ty) : Base{*ty} {}
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
    constexpr Z(Index ty, Square sq) : v_{::rotateleft(zKey[+ty], +sq)} {}

    constexpr _t operator + () const { return v_; }
    constexpr Z operator ~ () const { return Z{::byteswap(v_)}; }
    friend constexpr Z operator ^ (Z a, Z b) { return Z{a.v_ ^ b.v_}; }
    friend constexpr _t operator & (Z a, _t b) { return a.v_ & b; }

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
