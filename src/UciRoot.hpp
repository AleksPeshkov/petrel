#ifndef UCI_ROOT_HPP
#define UCI_ROOT_HPP

#include "io.hpp"
#include "typedefs.hpp"
#include "NodeRoot.hpp"

class Uci;

class UciRoot : public NodeRoot {
    ChessVariant chessVariant_ = Orthodox; //format of castling moves output
    index_t fullMoveNumber = 1; // number of full moves from the beginning of the game

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

    bool setCastling(Side, File);
    bool setCastling(Side, CastlingSide);
    bool setEnPassant(File);

    ostream& fen(ostream&) const;

public:
    UciRoot(const Uci& u) : NodeRoot{u} {}

    constexpr const ChessVariant& chessVariant() const { return chessVariant_; }
    void setChessVariant(ChessVariant v) { chessVariant_ = v; }

    void setStartpos();
    void readFen(istream&);
    void playMoves(istream&);
    void limitMoves(istream&);

    friend ostream& operator << (ostream& out, const UciRoot& pos) { return pos.fen(out); }
};

#endif
