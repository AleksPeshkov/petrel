#ifndef MOVE_HPP
#define MOVE_HPP

#include "typedefs.hpp"

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
