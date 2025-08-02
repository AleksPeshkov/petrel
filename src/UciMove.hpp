#ifndef UCI_MOVE_HPP
#define UCI_MOVE_HPP

#include "out.hpp"
#include "typedefs.hpp"

/**
 * Position independent move is 15 bits with the special move type flag to mark either castling, promotion or en passant move
 * and color of the side to move and chess variant to appropriate format of castling moves
 *
 * Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
 **/
class UciMove : public Move {
public:
    constexpr UciMove () : Move{} { static_assert (sizeof(UciMove) == sizeof(int16_t));}
    constexpr UciMove (Square f, Square t, bool s, Color c, ChessVariant v = Orthodox) : Move(f, t, s, c, v) {}

    friend out::ostream& operator << (out::ostream&, const UciMove&);
    friend out::ostream& operator << (out::ostream&, const UciMove[]);
};

#endif
