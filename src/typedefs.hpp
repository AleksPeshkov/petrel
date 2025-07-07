#ifndef TYPEDEFS_HPP
#define TYPEDEFS_HPP

#include <limits>
#include "types.hpp"
#include "Index.hpp"

typedef index_t ply_t; //search tree depth (relative to root)
enum : ply_t { MaxDepth = 127 };

typedef u64_t node_count_t;
enum : node_count_t {
    NodeCountNone = std::numeric_limits<node_count_t>::max(),
    NodeCountMax  = NodeCountNone - 1
};

enum file_t { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, };
typedef Index<8, file_t> File;

template <> inline
constexpr io::char_type File::to_char() const { return static_cast<io::char_type>('a' + this->v); }

template <> inline
bool File::from_char(io::char_type c) {
    File file{ static_cast<File::_t>(c - 'a') };
    if (!file.isValid()) { return false; }
    this->v = file;
    return true;
}

enum rank_t { Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1, };
typedef Index<8, rank_t> Rank;

constexpr Rank::_t rankForward(Rank rank) { return static_cast<Rank::_t>(rank + Rank8 - Rank7); }

template <> inline
constexpr io::char_type Rank::to_char() const { return static_cast<io::char_type>('8' - this->v); }

template <> inline
bool Rank::from_char(io::char_type c) {
    Rank rank{ static_cast<Rank::_t>('8' - c) };
    if (!rank.isValid()) { return false; }
    this->v = rank;
    return true;
}

enum color_t { White, Black };
typedef Index<2, color_t> Color;
template <> io::czstring Color::The_string;

enum side_to_move_t { My, Op };
typedef Index<2, side_to_move_t> Side;
constexpr Side::_t operator ~ (Side::_t my) { return static_cast<Side::_t>(my ^ Side::Mask); }

enum castling_side_t { KingSide, QueenSide };
typedef Index<2, castling_side_t> CastlingSide;
template <> io::czstring CastlingSide::The_string;

enum piece_index_t { TheKing };
typedef Index<16, piece_index_t> Pi; //piece index
template <> io::czstring Pi::The_string;

enum piece_type_t {
    Queen = 0,
    Rook = 1,
    Bishop = 2,
    Knight = 3,
    Pawn = 4,
    King = 5,
    ZobristCastling = 6,
    ZobristEnPassant = 7,
};
typedef Index<3, piece_type_t> SliderType;
typedef Index<4, piece_type_t> PromoType;
typedef Index<6, piece_type_t> PieceType;
typedef Index<8, piece_type_t> PieceZobristType;
template <> io::czstring PieceType::The_string;
template <> io::czstring PromoType::The_string;

constexpr bool isSlider(piece_type_t ty) { return ty < Knight; }
constexpr bool isLeaper(piece_type_t ty) { return ty >= Knight; }

/// encoding of the promoted piece type inside 12-bit move
constexpr Rank::_t rankOf(PromoType::_t ty) { return static_cast<Rank::_t>(ty); }

/// decoding promoted piece type from move destination square rank
constexpr PromoType::_t promoTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PromoType::_t>(r); }
constexpr PieceType::_t pieceTypeFrom(Rank::_t r) { assert (r < 4); return static_cast<PieceType::_t>(r); }

#endif
