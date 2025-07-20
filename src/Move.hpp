#ifndef MOVE_HPP
#define MOVE_HPP

#include "io.hpp"
#include "typedefs.hpp"
#include "Square.hpp"

enum chess_variant_t { Orthodox, Chess960 };
typedef Index<2, chess_variant_t> ChessVariant;

class Position;

/**
 * Internal move is 12 bits long (packed 'from' and 'to' squares) and linked to the position from it was made
 * Position independent move is 13 bits with the extra flag bit to mark either castling, promotion or en passant move
 * Any move's squares coordinates are relative to its side. Black side move should flip squares.
 *
 * Castling encoded as the castling rook moves over own king source square.
 * Pawn promotion piece type encoded in place of destination square rank.
 * En passant capture encoded as the pawn moves over captured pawn square.
 * Null move is encoded as 0 {A8A8}
 **/
class Move {
    enum Type { Simple, Special };

    struct _t {
        square_t from:6;
        square_t to:6;
        Type special:1;

        constexpr _t (Square f, Square t, Type s = Simple) : from{f}, to{t}, special{s} {}
    };

    union {
        unsigned n;
        _t v;
    };

    constexpr bool isSpecial() const { return v.special == Special; }

public:
    constexpr Move () : n{0} {} //null move
    constexpr Move(Square f, Square t, Type s = Simple) : v{f, t, s} {}

    Move(const Position& pos, Move move) : Move{pos, move.from(), move.to()} {}
    Move (const Position&, Square, Square);

    // change move squares to black side
    constexpr Move operator ~ () const { return Move{~from(), ~to(), static_cast<Move::Type>(isSpecial())}; }

    constexpr operator bool() const { return n != 0; }

    constexpr Square from() const { return v.from; }
    constexpr Square to()   const { return v.to; }

    static io::ostream& write(io::ostream&, Move, Color, ChessVariant = Orthodox);
    static io::ostream& write(io::ostream&, const Move[], Color, ChessVariant = Orthodox);
};

#endif
