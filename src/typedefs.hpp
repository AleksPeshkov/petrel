#ifndef TYPEDEFS_HPP
#define TYPEDEFS_HPP

#include <limits>
#include "types.hpp"
#include "Index.hpp"
#include "Square.hpp"

// search tree distance in halfmoves
struct Ply : Index<64> {
    using Index::Index;

    Ply(signed ply) : Index{static_cast<_t>(ply)} {}
    Ply(index_t ply) : Index{ply} {}

    friend ostream& operator << (ostream& out, Ply ply) { return out << static_cast<unsigned>(ply); }
    friend istream& operator >> (istream& in, Ply& ply) {
        index_t n = Ply::Last;
        in >> n;
        ply = { std::min(n, static_cast<index_t>(Ply::Last)) };
        return in;
    }
};
constexpr Ply::_t MaxPly = Ply::Last; // Ply is limited to [0 .. MaxPly]

typedef u8_t MovesNumber; // number of (legal) moves in the position

typedef u64_t node_count_t;
enum : node_count_t {
    NodeCountNone = std::numeric_limits<node_count_t>::max(),
    NodeCountMax  = NodeCountNone - 1
};

enum class NodeControl {
    Continue,
    Abort,
    BetaCutoff,
};

// number of halfmoves without capture or pawn move
class Rule50 : Index<101> {
public:
    using Index::operator const _t&;
    constexpr Rule50() : Index{0} {}
    constexpr void clear() { v = 0; }
    constexpr void next() { v = v < Last ? v + 1 : static_cast<_t>(Last); }
    constexpr bool isEmpty() const { return v == 0; }
    constexpr bool isDraw() const { return v == Last; }

    friend istream& operator >> (istream& in, Rule50& rule50) {
        auto beforeRule50 = in.tellg();
        unsigned _rule50 = 0; // default value
        in >> _rule50;
        if (_rule50 > Last) { return io::fail_pos(in, beforeRule50); }
        rule50.v = _rule50;
        return in;
    }

    friend ostream& operator << (ostream& out, const Rule50& rule50) {
        return out << rule50.v;
    }
};

enum color_t { White, Black };
typedef Index<2, color_t> Color;
template <> io::czstring Color::The_string;

// color to move of the given ply
constexpr Color operator << (Color c, Ply ply) { return static_cast<Color::_t>((ply ^ static_cast<unsigned>(c)) & Color::Mask); }

enum side_to_move_t {
    My, // side to move
    Op  // opposite to side to move
};
typedef Index<2, side_to_move_t> Side;
constexpr Side::_t operator ~ (Side::_t my) { return static_cast<Side::_t>(my ^ static_cast<Side::_t>(Side::Mask)); }

enum chess_variant_t { Orthodox, Chess960 };
typedef Index<2, chess_variant_t> ChessVariant;

enum castling_side_t { KingSide, QueenSide };
typedef Index<2, castling_side_t> CastlingSide;
template <> io::czstring CastlingSide::The_string;

enum piece_index_t { TheKing }; // king index is always 0
typedef Index<16, piece_index_t> Pi; //piece index 0..15
template <> io::czstring Pi::The_string;

enum piece_type_t {
    Queen = 0,
    Rook = 1,
    Bishop = 2,
    Knight = 3,
    Pawn = 4,
    King = 5,
};
typedef Index<3, piece_type_t> SliderType; // Queen, Rook, Bishop
typedef Index<4, piece_type_t> PromoType; // Queen, Rook, Bishop, Knight
typedef Index<6, piece_type_t> PieceType; // Queen, Rook, Bishop, Knight, Pawn, King
template <> io::czstring PieceType::The_string;
template <> io::czstring PromoType::The_string;

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; } // Queen, Rook, Bishop
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; } // Knight, Pawn, King

// encoding of the promoted piece type inside 12-bit move
constexpr Rank::_t rankOf(PromoType::_t ty) { return static_cast<Rank::_t>(ty); }

// decoding promoted piece type from move destination square rank
constexpr PromoType::_t promoTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PromoType::_t>(r); }

// encoding piece type from move destination square rank
constexpr PieceType::_t pieceTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PieceType::_t>(r); }

enum class ReturnStatus {
    Continue,   // continue search normally
    Stop,       // immediately stop current
    BetaCutoff, // inform about beta cuttoff to prune current node
};

#define RETURN_IF_STOP(visitor) { if (visitor == ReturnStatus::Stop) { return ReturnStatus::Stop; } } ((void)0)

/**
 * Internal move is 12 bits long (packed 'from' and 'to' squares) and linked to the position from it was made
 *
 * Castling encoded as the castling rook moves over own king source square.
 * Pawn promotion piece type encoded in place of destination square rank.
 * En passant capture encoded as the pawn moves over captured pawn square.
 * Null move is encoded as 0 {A8A8}
 **/
class PACKED Move {
public:
    enum move_type_t {
        Normal, // normal move or capture
        Special // castling, promotion or en passant capture
    };
    typedef Index<2, move_type_t> MoveType;

protected:
    Square::_t from_:6 = static_cast<Square::_t>(0);
    Square::_t to_:6 = static_cast<Square::_t>(0);

    // used by UciMove declared here to pack all bit fields
    Color::_t color:1 = White;
    ChessVariant::_t variant:1 = Orthodox;
    MoveType::_t type:1 = Normal;

    // UciMove constructor
    constexpr Move(Square f, Square t, bool s, Color c, ChessVariant v)
        : from_{f}, to_{t}, color{c}, variant{v}, type{s ? Special : Normal} {}

public:
    // null move
    constexpr Move () = default;

    constexpr Move (Square f, Square t) : from_{f}, to_{t} { static_assert (sizeof(Move) == sizeof(int16_t)); }

    // check if move is not null
    constexpr operator bool() const { return !(from_ == 0 && to_ == 0); }

    // source square the piece moved from
    constexpr Square from() const { return from_; }

    // destination square the piece moved to
    constexpr Square to() const { return to_; }

    friend constexpr bool operator == (Move a, Move b) { return a.from_ == b.from_ && a.to_ == b.to_; }
    friend constexpr bool operator != (Move a, Move b) { return !(a == b); }
};

#endif
