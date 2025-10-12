#ifndef UCI_MOVE_HPP
#define UCI_MOVE_HPP

#include "io.hpp"
#include "typedefs.hpp"

/**
 * Position independent move is 15 bits with the special move type flag to mark either castling, promotion or en passant move
 * and color of the side to move and chess variant to appropriate format of castling moves
 *
 * Any move's squares coordinates are relative to its side. Black side's move should flip squares before printing.
 **/
class UciMove {
    enum move_type_t : u8_t {
        Normal, // normal move or capture
        Special // castling, promotion or en passant capture
    };
    using MoveType = Index<2, move_type_t>;

protected:
    Square::_t from_:6 = static_cast<Square::_t>(0);
    Color::_t color:1 = White;
    ChessVariant::_t variant:1 = Orthodox;
    Square::_t to_:6 = static_cast<Square::_t>(0);
    MoveType::_t type:1 = Normal;

public:
    constexpr UciMove () = default;
    constexpr UciMove (Square f, Square t, bool s, Color c, ChessVariant v = ChessVariant{Orthodox})
        : from_{f}, color{c}, variant{v}, to_{t}, type{s ? Special : Normal}
    {
        static_assert (sizeof(UciMove) == sizeof(int16_t));
    }

    constexpr operator Move () const { return Move{from_, to_}; }

    // check if move is not null
    constexpr operator bool() const { return !(from_ == 0 && to_ == 0); }

    // source square the piece moved from
    constexpr Square from() const { return Square{from_}; }

    // destination square the piece moved to
    constexpr Square to() const { return Square{to_}; }

    friend ostream& operator << (ostream&, const UciMove&);
    friend ostream& operator << (ostream&, const UciMove[]);
};

#endif
