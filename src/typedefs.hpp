#ifndef TYPEDEFS_HPP
#define TYPEDEFS_HPP

#include <limits>
#include "Index.hpp"
#include "Square.hpp"

// search tree distance in halfmoves
class Ply : public Index<64, u8_t> {
public:
    constexpr Ply(int i = 0) : Index{static_cast<_t>(i > 0 ? i : 0)} { assertOk(); }
    friend bool operator < (Index a, int i) { return a < i; }
    friend bool operator <= (Index a, int i) { return a <= i; }
};
constexpr Ply::_t MaxPly = Ply::Last; // Ply is limited to [0 .. MaxPly]

using MovesNumber = int; // number of (legal) moves in the position

using node_count_t = u64_t;
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
class Rule50 {
    int v;
    static constexpr int Draw = 100;
public:
    constexpr Rule50() : v{0} {}
    constexpr void clear() { v = 0; }
    constexpr void next() { v = v < Draw ? v + 1 : Draw; }
    constexpr bool isDraw() const { return v == Draw; }

    friend constexpr bool operator < (const Rule50& rule50, int ply) { return rule50.v < ply; }

    friend ostream& operator << (ostream& out, const Rule50& rule50) { return out << rule50.v; }

    friend istream& operator >> (istream& in, Rule50& rule50) {
        in >> rule50.v;
        if (in) { assert (0 <= rule50.v && rule50.v <= 100); }
        return in;
    }
};

enum color_t : u8_t { White, Black };
template <> struct CharMap<color_t> { static constexpr io::czstring The_string = "wb"; };
using Color = IndexChar<2, color_t>;

// color to move of the given ply
constexpr Color::_t distance(Color c, Ply ply) { return static_cast<Color::_t>((ply ^ static_cast<unsigned>(c)) & Color::Mask); }

enum side_to_move_t {
    My, // side to move
    Op  // opposite to side to move
};
using Side = Index<2, side_to_move_t>;
constexpr Side::_t operator ~ (Side::_t si) { return static_cast<Side::_t>(si ^ static_cast<Side::_t>(Side::Mask)); }

enum chess_variant_t : u8_t { Orthodox, Chess960 };
using ChessVariant = Index<2, chess_variant_t>;

enum castling_side_t { KingSide, QueenSide };
template <> struct CharMap<castling_side_t> { static constexpr io::czstring The_string = "kq"; };
using CastlingSide = IndexChar<2, castling_side_t>;

enum piece_index_t { TheKing, Last = 15 }; // king index is always 0
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

using SliderType = Index<3, piece_type_t>; // Queen, Rook, Bishop
using PromoType = IndexChar<4, piece_type_t>; // Queen, Rook, Bishop, Knight
using NonKingType = Index<5, piece_type_t>; // Queen, Rook, Bishop, Knight, Pawn
using PieceType = IndexChar<6, piece_type_t>; // Queen, Rook, Bishop, Knight, Pawn, King

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; } // Queen, Rook, Bishop
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; } // Knight, Pawn, King

// encoding of the promoted piece type inside 12-bit move
constexpr Rank::_t rankOf(PromoType::_t ty) { return static_cast<Rank::_t>(ty); }

// decoding promoted piece type from move destination square rank
constexpr PromoType::_t promoTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PromoType::_t>(r); }

// encoding piece type from move destination square rank
constexpr PieceType::_t pieceTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PieceType::_t>(r); }

// continue or stop search
enum class ReturnStatus {
    Continue,   // continue search normally
    Stop,       // stop current search (timeout or other termination reason)
    BetaCutoff, // prune current node search
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
class Move {
protected:
    Square::_t from_ = static_cast<Square::_t>(0);
    Square::_t to_ = static_cast<Square::_t>(0);

public:
    // null move
    constexpr Move () = default;

    constexpr Move (Square::_t f, Square::_t t) : from_{f}, to_{t} { static_assert (sizeof(Move) == sizeof(int16_t)); }

    // check if move is not null
    constexpr operator bool() const { return !(from_ == 0 && to_ == 0); }

    // source square the piece moved from
    constexpr Square from() const { return Square{from_}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{to_}; }

    friend constexpr bool operator == (Move a, Move b) { return a.from_ == b.from_ && a.to_ == b.to_; }
};

#endif
