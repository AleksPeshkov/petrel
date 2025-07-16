#ifndef MOVE_HPP
#define MOVE_HPP

#include "out.hpp"
#include "typedefs.hpp"

enum chess_variant_t { Orthodox, Chess960 };
typedef Index<2, chess_variant_t> ChessVariant;

/**
 * Internal move is 12 bits long (packed 'from' and 'to' squares) and linked to the position from it was made
 * Position independent move is 14 bits with the special move type flag to mark either castling, promotion or en passant move
 * and color of the side to move and chess variant to appropriate format of castling moves
 *
 * Any move's squares coordinates are relative to its side. Black side move should flip squares.
 *
 * Castling encoded as the castling rook moves over own king source square.
 * Pawn promotion piece type encoded in place of destination square rank.
 * En passant capture encoded as the pawn moves over captured pawn square.
 * Null move is encoded as 0 {A8A8}
 **/
class Move {
    enum move_type_t {
        Internal,    // move has only minimal info and cannot be corretly printed
        Normal  = 2, // normal move with full info (color and chess variant)
        Special = 3  // castling, promotion or en passant move with full info (color and chess variant)
    };
    typedef Index<2, move_type_t> MoveType;

    Square::_t _from:6;
    Square::_t _to:6;
    Color::_t color:1;
    ChessVariant::_t variant:1;
    MoveType::_t type:2;

public:
    // null move
    constexpr Move () : _from{static_cast<Square::_t>(0)}, _to{static_cast<Square::_t>(0)}, color{White}, variant{Orthodox}, type{Internal} {}

    // internal move
    constexpr Move (Square from, Square to) : _from{from}, _to{to}, color{White}, variant{Orthodox}, type{Internal} {}

    // full move
    constexpr Move(Square from, Square to, bool isSpecial, Color c, ChessVariant v = Orthodox)
        : _from{from}, _to{to}, color{c}, variant{v}, type{isSpecial ? Special : Normal} {}

    // check if move is not null
    constexpr operator bool() const { return !(_from == 0 && _to == 0); }

    constexpr bool isExternal() const { return type != Internal; }
    constexpr bool isSpecial()  const { return type == Special; }

    // square from which move is made
    constexpr Square from() const { return _from; }

    // square to which move is made
    constexpr Square to() const { return _to; }

    friend constexpr bool operator == (Move a, Move b) { return a._from == b._from && a._to == b._to; }
    friend constexpr bool operator != (Move a, Move b) { return !(a == b); }

    friend out::ostream& operator << (out::ostream&, Move);
    friend out::ostream& operator << (out::ostream&, const Move[]);
};

#endif
