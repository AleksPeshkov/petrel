#ifndef UCI_ROOT_HPP
#define UCI_ROOT_HPP

#include "Index.hpp"
#include "PositionMoves.hpp"

class Uci;
class Repetitions;

class UciPosition : public PositionMoves {
    int fullMoveNumber = 1; // number of full moves from the beginning of the game
    Color colorToMove_{White}; //root position side to move color
    ChessVariant chessVariant_{Orthodox}; // castling moves and fen output format, engine accepts any castling input

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

    bool setEnPassant(File);

    ostream& fen(ostream&) const;

public:
    void setStartpos();
    void readFen(istream&);
    void playMoves(istream&, Repetitions&);
    void limitMoves(istream&);

    constexpr Side sideOf(Color::_t color) const { return Side{colorToMove_.is(color) ? My : Op}; }
    constexpr Color colorToMove(Ply ply) const { return Color{ ::distance(colorToMove_, ply) }; }
    constexpr ChessVariant chessVariant() const { return chessVariant_; }
    constexpr void setChessVariant(ChessVariant v) { chessVariant_ = v; }

    friend ostream& operator << (ostream& out, const UciPosition& pos) { return pos.fen(out); }
};

#endif
