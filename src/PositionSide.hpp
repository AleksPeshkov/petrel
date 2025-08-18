#ifndef POSITION_SIDE_HPP
#define POSITION_SIDE_HPP

#include "Evaluation.hpp"
#include "PiBbMatrix.hpp"
#include "PiSquare.hpp"
#include "PiTrait.hpp"
#include "PiType.hpp"

//TRICK: all squares are relative to its own side (so the king piece is initially on E1 square regardless color)

/// static information about pieces from one player's side (either side to move or its opponent)
class PositionSide {
    PiBbMatrix attacks_; // squares attacked by a piece and pieces attacking to a square
    PiType types; // chess type of each alive piece: king, pawn, knignt, bishop, rook, queen
    PiTrait traits; // rooks with castling rights, pawns affected by en passant, pinner pieces, checker pieces
    PiSquare squares; // onboard square locations of the alive pieces or 'NoSquare' special value

    Bb bbSide_; // squares of all current side pieces
    Bb bbPawns_; // squares of current side pawns

    Evaluation evaluation_; // PST incremental evaluation
    Square opKing; // square of the opponent's king (from current side point of view)

    void move(Pi, PieceType, Square, Square);
    void updateMovedKing(Square);
    void setLeaperAttacks();
    void setLeaperAttack(Pi, PieceType, Square);
    void setPinner(Pi, PieceType, Square);

    #ifdef NDEBUG
        void assertOk(Pi) const {}
        void assertOk(Pi, PieceType, Square) const {}
    #else
        void assertOk(Pi) const;
        void assertOk(Pi, PieceType, Square) const;
    #endif

public:
    constexpr const PiBbMatrix& attacks() const { return attacks_; }

    // bitboard of squares occupied by all current side pieces
    constexpr const Bb& bbSide() const { return bbSide_; }

    // bitboard of squares occupied by current side pawns
    constexpr const Bb& bbPawns() const { return bbPawns_; }

    // static evaluation data of pieces and piece-squares
    constexpr const Evaluation& evaluation() const { return evaluation_; }

    bool has(Square sq) const { assert (bbSide_.has(sq) == squares.has(sq)); return bbSide_.has(sq); }
    Square squareOf(Pi pi) const { assertOk(pi); return squares.squareOf(pi); }
    Square kingSquare() const { return squareOf(TheKing); }

    // mask of all pieces of the given side
    PiMask pieces() const { assert (squares.pieces() == types.pieces()); return squares.pieces(); }
    PiMask sliders() const { return types.sliders(); }

    Pi pieceAt(Square sq) const { Pi pi = squares.pieceAt(sq); assertOk(pi); return pi; }
    PiMask piecesOn(Rank rank) const { return squares.piecesOn(rank); }

    PiMask piecesOfType(PieceType ty) const { return types.piecesOfType(ty); }
    PieceType typeOf(Pi pi) const { assertOk(pi); return types.typeOf(pi); }
    PieceType typeOf(Square sq) const { return typeOf(pieceAt(sq)); }

    PiMask pawns() const { return types.piecesOfType(Pawn); }
    bool isPawn(Pi pi) const { assertOk(pi); return types.isPawn(pi); }

    PiMask goodKillers(PieceType ty) const { return types.goodKillers(ty); }
    PiMask notBadKillers(PieceType ty) const { return types.notBadKillers(ty); }

    PiMask castlingRooks() const { return traits.castlingRooks(); }
    bool isCastling(Pi pi) const { assertOk(pi); return traits.isCastling(pi); }
    bool isCastling(Square sq) const { return isCastling(pieceAt(sq)); }

    PiMask enPassantPawns() const { return traits.enPassantPawns(); }
    bool hasEnPassant() const { return enPassantPawns().any(); }
    Square enPassantSquare() const { Square ep = squareOf(traits.getEnPassant()); assert (ep.on(Rank4)); return ep; }
    File enPassantFile() const { return File( enPassantSquare() ); }

    PiMask pinners() const { return traits.pinners(); }
    bool isPinned(Bb) const;

    PiMask checkers() const { assert (traits.checkers() == attacks_[opKing]); return traits.checkers(); }

    PiMask promotables() const { return traits.promotables(); }
    bool isPromotable(Pi pi) const { assertOk(pi); return traits.isPromotable(pi); }

    Bb attacksOf(Pi pi) const { return attacks_[pi]; }
    PiMask attackersTo(Square sq) const { return attacks_[sq]; }
    PiMask affectedBy(Square sq) const { return attackersTo(sq); }
    PiMask affectedBy(Square a, Square b) const { return affectedBy(a) | affectedBy(b); }
    PiMask affectedBy(Square a, Square b, Square c) const { return affectedBy(a) | affectedBy(b) | affectedBy(c); }

//friend class Position;
    static void swap(PositionSide&, PositionSide&);

    void setOpKing(Square);
    void move(Pi, Square, Square);
    void movePawn(Pi, Square, Square);
    void moveKing(Square, Square);
    void castle(Square kingFrom, Square kingTo, Pi rook, Square rookFrom, Square rookTo);
    Pi promote(Pi, Square, PromoType, Square);
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

// friend class UciRoot;
    bool setValidCastling(File);
    bool setValidCastling(CastlingSide);

};

#endif
