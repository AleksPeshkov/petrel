#ifndef POSITION_SIDE_HPP
#define POSITION_SIDE_HPP

#include "Evaluation.hpp"
#include "PiBb.hpp"
#include "PiTrait.hpp"
#include "PiType.hpp"
#include "PiSquare.hpp"
#include "Zobrist.hpp"

//TRICK: all squares are relative to its own side (so the king piece is initially on E1 square regardless color)

/// static information about pieces from one player's side (either side to move or its opponent)
class PositionSide {
    PiBb attacks; //squares attacked by a piece and pieces attacking to a square
    PiType types; //chess type of each alive piece: king, pawn, knignt, bishop, rook, queen
    PiTrait traits; //rooks with castling rights, pawns affected by en passant, pinner pieces, checker pieces
    PiSquare squares; //onboard square locations of the alive pieces or 'NoSquare' special value

    Bb piecesBb; //squares of current side pieces
    Bb pawnsBb; //squares of current side pawns

    Evaluation evaluation; //PST incremental evaluation

    Square opKing; //location of the opponent's king, needed for detecting checking and pinning pieces traits

public:
    #ifdef NDEBUG
        void assertOk(Pi) const {}
        void assertOk(Pi, PieceType, Square) const {}
    #else
        void assertOk(Pi) const;
        void assertOk(Pi, PieceType, Square) const;
    #endif

    // bitboard of squares occupied by current side pieces
    const Bb& piecesSquares() const { return piecesBb; }

    // bitboard of squares occupied by current side pawns
    const Bb& pawnsSquares() const { return pawnsBb; }

    bool has(Square sq) const { assert (piecesBb.has(sq) == squares.has(sq)); return piecesBb.has(sq); }

    // mask of all pieces of the given side
    PiMask pieces() const { assert (squares.pieces() == types.pieces()); return squares.pieces(); }
    PiMask sliders() const { return types.sliders(); }

    Square squareOf(Pi pi) const { assertOk(pi); return squares.squareOf(pi); }
    Square kingSquare() const { return squareOf(TheKing); }

    Pi pieceAt(Square sq) const { Pi pi = squares.pieceAt(sq); assertOk(pi); return pi; }
    PiMask piecesOn(Rank rank) const { return squares.piecesOn(rank); }

    PiMask piecesOfType(PieceType ty) const { return types.piecesOfType(ty); }
    PieceType typeOf(Pi pi) const { assertOk(pi); return types.typeOf(pi); }
    PieceType typeOf(Square sq) const { return typeOf(pieceAt(sq)); }

    PiMask pawns() const { return types.piecesOfType(Pawn); }
    bool isPawn(Pi pi) const { assertOk(pi); return types.isPawn(pi); }

    PiMask castlingRooks() const { return traits.castlingRooks(); }
    bool isCastling(Pi pi) const { assertOk(pi); return traits.isCastling(pi); }
    bool isCastling(Square sq) const { return isCastling(pieceAt(sq)); }

    PiMask enPassantPawns() const { return traits.enPassantPawns(); }
    bool hasEnPassant() const { return enPassantPawns().any(); }
    Square enPassantSquare() const { Square ep = squareOf(traits.getEnPassant()); assert (ep.on(Rank4)); return ep; }
    File enPassantFile() const { return File( enPassantSquare() ); }

    PiMask pinners() const { return traits.pinners(); }
    bool isPinned(Bb) const;

    PiMask checkers() const { assert (traits.checkers() == attacks[opKing]); return traits.checkers(); }

    PiMask promotables() const { return traits.promotables(); }
    bool isPromotable(Pi pi) const { assertOk(pi); return traits.isPromotable(pi); }

    const PiBb& attacksMatrix() const { return attacks; }
    PiMask attackersTo(Square sq) const { return attacks[sq]; }
    PiMask affectedBy(Square sq) const { return attackersTo(sq); }
    PiMask affectedBy(Square a, Square b) const { return affectedBy(a) | affectedBy(b); }
    PiMask affectedBy(Square a, Square b, Square c) const { return affectedBy(a) | affectedBy(b) | affectedBy(c); }

    static Score evaluate(const PositionSide&, const PositionSide&);

    //calculate Zobrist key from scratch
    Zobrist generateZobrist() const;

private:
    void move(Pi, PieceType, Square, Square);
    void updateMovedKing(Square);
    void setLeaperAttacks();
    void setLeaperAttack(Pi, PieceType, Square);
    void setPinner(Pi, PieceType, Square);

friend class Position;

    static void swap(PositionSide&, PositionSide&);

    void setOpKing(Square);
    void move(Pi, Square, Square);
    void movePawn(Pi, Square, Square);
    void moveKing(Square, Square);
    void castle(Square kingFrom, Square kingTo, Pi rook, Square rookFrom, Square rookTo);
    void promote(Pi, Square, PromoType, Square);
    void capture(Square);

    void setEnPassantVictim(Pi);
    void setEnPassantKiller(Pi);
    void clearEnPassantVictim();
    void clearEnPassantKillers();
    void clearCheckers() { traits.clearCheckers(); }

    void updateSliders(PiMask, Bb);
    void updateSlidersCheckers(PiMask, Bb);

    //used only during initial position setup
    bool dropValid(PieceType, Square);
    static void finalSetup(PositionSide&, PositionSide&);

friend class PositionFen;

    bool setValidCastling(File);
    bool setValidCastling(CastlingSide);

};

#endif
