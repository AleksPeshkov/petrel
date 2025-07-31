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
    Ply(unsigned ply) : Index{static_cast<_t>(ply)} {}

    friend io::ostream& operator << (io::ostream& out, Ply ply) { return out << static_cast<unsigned>(ply); }
};
constexpr Ply::_t MaxPly = Ply::Size - 1; // Ply is limited to [0 .. MaxPly]

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

/**
 * Internal move is 12 bits long (packed 'from' and 'to' squares) and linked to the position from it was made
 *
 * Castling encoded as the castling rook moves over own king source square.
 * Pawn promotion piece type encoded in place of destination square rank.
 * En passant capture encoded as the pawn moves over captured pawn square.
 * Null move is encoded as 0 {A8A8}
 **/
class Move {
    Square::_t _from:6;
    Square::_t _to:6;

public:
    // null move
    constexpr Move () : _from{static_cast<Square::_t>(0)}, _to{static_cast<Square::_t>(0)} {}

    constexpr Move (Square f, Square t) : _from{f}, _to{t} {}

    // check if move is not null
    constexpr operator bool() const { return !(_from == 0 && _to == 0); }

    // source square the piece moved from
    constexpr Square from() const { return _from; }

    // destination square the piece moved to
    constexpr Square to() const { return _to; }

    friend constexpr bool operator == (Move a, Move b) { return a._from == b._from && a._to == b._to; }
    friend constexpr bool operator != (Move a, Move b) { return !(a == b); }
};

#endif
