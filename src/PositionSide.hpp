#ifndef POSITION_SIDE_HPP
#define POSITION_SIDE_HPP

#include "Evaluation.hpp"
#include "PiBbMatrix.hpp"
#include "PiSquare.hpp"
#include "PiTrait.hpp"
#include "PiType.hpp"


// static information about pieces from one player's side (either side to move or its opponent)
// TRICK: all squares always relative to the point of view of given side
// (so the king piece is initially on E1 square regardless color)
class PositionSide {
    PiBbMatrix attacks_; // squares attacked by a piece and pieces attacking to a square
    PiType types; // chess type of each alive piece: king, pawn, knignt, bishop, rook, queen
    PiTrait traits; // rooks with castling rights, pawns affected by en passant, pinner pieces, checker pieces
    PiSquare squares; // onboard square locations of the alive pieces or 'NoSquare' special value

    Bb bbSide_; // bitboard of squares of all current side pieces
    Bb bbPawns_; // bitboard of squares of current side pawns
    Bb bbPawnAttacks_; // bitboard of squares attacked by pawns

    Evaluation evaluation_; // PST incremental evaluation
    Square opKing; // square of the opponent's king (from current side point of view)

    #ifdef NDEBUG
        void assertOk(Pi) const {}
        void assertOk(Pi, PieceType, Square) const {}
    #else
        void assertOk(Pi) const;
        void assertOk(Pi, PieceType, Square) const;
    #endif

    void move(Pi, PieceType, Square, Square);
    void updateMovedKing(Square);
    void setLeaperAttacks();
    void setLeaperAttack(Pi, PieceType, Square);
    void setPinner(Pi, SliderType, Square);

public:
    constexpr const PiBbMatrix& attacks() const { return attacks_; }

    // bitboard of squares occupied by the given side pieces
    constexpr const Bb& bbSide() const { return bbSide_; }

    // bitboard of squares occupied by the given side pawns
    constexpr const Bb& bbPawns() const { return bbPawns_; }

    // bitboard of squares attacked by the given side pawns
    constexpr const Bb& bbPawnAttacks() const { assert (bbPawnAttacks_ == bbPawns_.pawnAttacks()); return bbPawnAttacks_; }

    // static evaluation data of the given side pieces
    constexpr const Evaluation& evaluation() const { return evaluation_; }

    bool has(Square sq) const { assert (bbSide_.has(sq) == squares.has(sq)); return bbSide_.has(sq); }
    Square squareOf(Pi pi) const { assertOk(pi); return squares.squareOf(pi); }
    Square kingSquare() const { return squareOf(Pi{TheKing}); }

    // mask of all pieces of the given side
    PiMask pieces() const { assert (squares.pieces() == types.pieces()); return squares.pieces(); }
    PiMask sliders() const { return types.sliders(); }
    PiMask figures() const { return types.figures(); }
    PiMask notPawns() const { return types.notPawns(); }
    PiMask notKing() const { return types.notKing(); }

    Pi pieceAt(Square sq) const { assert (has(sq)); Pi pi = squares.pieceAt(sq); assertOk(pi); return pi; }
    PiMask piecesOn(Rank rank) const { return squares.piecesOn(rank); }

    PiMask piecesOfType(PieceType ty) const { return types.piecesOfType(ty); }
    PieceType typeOf(Pi pi) const { assertOk(pi); return types.typeOf(pi); }
    PieceType typeAt(Square sq) const { return typeOf(pieceAt(sq)); }
    Score score(PieceType ty, Square sq) const { return evaluation().score(ty, sq); }
    Score scoreAt(Square sq) const { return score(typeOf(pieceAt(sq)), sq); }

    PiMask pawns() const { return types.piecesOfType(Pawn); }
    bool isPawn(Pi pi) const { assertOk(pi); return types.isPawn(pi); }

    // pieces of less valuable types than given piece type
    PiMask lessValue(PieceType ty) const { return types.lessValue(ty); }

    // pieces of less or equal value than given piece type
    PiMask lessOrEqualValue(PieceType ty) const { return types.lessOrEqualValue(ty); }

    PiMask castlingRooks() const { return traits.castlingRooks(); }
    bool isCastling(Pi pi) const { assertOk(pi); return traits.isCastling(pi); }
    bool isCastling(Square sq) const { return isCastling(pieceAt(sq)); }

    PiMask enPassantPawns() const { return traits.enPassantPawns(); }
    bool isEnPassant(Pi pi) const { return traits.isEnPassant(pi); }
    bool hasEnPassant() const { return enPassantPawns().any(); }
    Square enPassantSquare() const { Square ep = squareOf(traits.getEnPassant()); assert (ep.on(Rank4)); return ep; }
    File enPassantFile() const { return File{enPassantSquare()}; }

    PiMask pinners() const { return traits.pinners(); }
    bool isPinned(Bb) const;

    PiMask checkers() const { assert (traits.checkers() == attacks_[opKing]); return traits.checkers(); }

    // pawns on the 7th rank
    PiMask promotables() const { return traits.promotables(); }

    // is pawn and pawn is on the 7th rank
    bool isPromotable(Pi pi) const { assertOk(pi); return traits.isPromotable(pi); }

    Bb attacksOf(Pi pi) const { assertOk(pi); return attacks_[pi]; }
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

// friend class UciPosition;
    bool setValidCastling(File);
    bool setValidCastling(CastlingSide);

};

#endif
