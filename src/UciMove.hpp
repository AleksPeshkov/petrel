#ifndef UCI_MOVE_HPP
#define UCI_MOVE_HPP

#include "out.hpp"
#include "Move.hpp"

enum chess_variant_t { Orthodox, Chess960 };
typedef Index<2, chess_variant_t> ChessVariant;

/**
 * Position independent move is 15 bits with the special move type flag to mark either castling, promotion or en passant move
 * and color of the side to move and chess variant to appropriate format of castling moves
 *
 * Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
 **/
class UciMove : public Move {
    enum move_type_t {
        Normal, // normal move or capture
        Special // castling, promotion or en passant capture
    };
    typedef Index<2, move_type_t> MoveType;

    Color::_t color:1;
    ChessVariant::_t variant:1;
    MoveType::_t type:1;

public:
    // null move
    constexpr UciMove () : Move{}, color{White}, variant{Orthodox}, type{Normal} {}

    constexpr UciMove(Square from, Square to, bool isSpecial, Color c, ChessVariant v = Orthodox)
        : Move{from, to}, color{c}, variant{v}, type{isSpecial ? Special : Normal} {}

    friend out::ostream& operator << (out::ostream&, const UciMove&);
    friend out::ostream& operator << (out::ostream&, const UciMove[]);
};

#endif
