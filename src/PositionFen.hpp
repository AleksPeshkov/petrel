#ifndef POSITION_FEN_HPP
#define POSITION_FEN_HPP

#include "io.hpp"
#include "typedefs.hpp"
#include "PositionMoves.hpp"
#include "UciMove.hpp"

using in::istream;
using out::ostream;

class RepetitionHistory;

class PositionFen : public PositionMoves {
    Color colorToMove = White; //root position color for moves long algebraic format output
    ChessVariant chessVariant = Orthodox; //format of castling moves output

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

    bool setCastling(Side, File);
    bool setCastling(Side, CastlingSide);
    bool setEnPassant(File);

    ostream& writeFen(ostream&) const;

public:
    constexpr Side sideOf(Color color) const { return colorToMove.is(color) ? My : Op; }
    constexpr Color getColorToMove(Ply ply = 0) const { return colorToMove << ply; }

    constexpr bool isChess960() const { return chessVariant.is(Chess960); }
    constexpr const ChessVariant& getChessVariant() const { return chessVariant; }
    void setChessVariant(ChessVariant v) { chessVariant = v; }
    void setStartpos(RepetitionHistory&);
    void readFen(istream&, RepetitionHistory&);
    void playMoves(istream&, RepetitionHistory&);
    void limitMoves(istream&);

    friend ostream& operator << (ostream& out, const PositionFen& pos) { return pos.writeFen(out); }
};

#endif
