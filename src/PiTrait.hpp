#ifndef PI_TRAIT_H
#define PI_TRAIT_H

#include "typedefs.hpp"
#include "PiBit.hpp"

enum class Trait {
    Castling,  // rooks with castling rights
    EnPassant, // pawn can be captured en passant or pawn or pawns that can perform a legal en passant capture (depends on side to move)
    Pinner,    // sliding pieces that can attack the enemy king on empty board (potential pinners)
    Checker,   // any piece actually attacking enemy king
    Promotable // pawns on the 7th rank
};

class PiTrait : protected PiBit< PiTrait, Index<4, Trait> > {
    typedef PiBit< PiTrait, Index<4, Trait> > Base;
public:
    using Base::clear;
    using Base::isEmpty;

    PiMask castlingRooks() const { return anyOf(Trait::Castling); }
    bool isCastling(Pi pi) const { return is(pi, Trait::Castling); }
    void setCastling(Pi pi) { assert (!isCastling(pi)); set(pi, Trait::Castling); }
    void clearCastlings() { clear(Trait::Castling); }

    PiMask enPassantPawns() const { return anyOf(Trait::EnPassant); }
    Pi getEnPassant() const { Pi pi = enPassantPawns().index(); return pi; }
    bool isEnPassant(Pi pi) const { return is(pi, Trait::EnPassant); }
    void setEnPassant(Pi pi) { set(pi, Trait::EnPassant); }
    void clearEnPassant(Pi pi) { assert (isEnPassant(pi)); clear(pi, Trait::EnPassant); }
    void clearEnPassants() { clear(Trait::EnPassant); }

    PiMask pinners() const { return anyOf(Trait::Pinner); }
    bool isPinner(Pi pi) const { return is(pi, Trait::Pinner); }
    void clearPinners() { clear(Trait::Pinner); }
    void setPinner(Pi pi) { assert (!isPinner(pi)); set(pi, Trait::Pinner); }
    void clearPinner(Pi pi) { clear(pi, Trait::Pinner); }

    PiMask checkers() const { return anyOf(Trait::Checker); }
    void clearCheckers() { clear(Trait::Checker); }
    void setChecker(Pi pi) { assert (!is(pi, Trait::Checker)); set(pi, Trait::Checker); }

    PiMask promotables() const { return anyOf(Trait::Promotable); }
    bool isPromotable(Pi pi) const { return is(pi, Trait::Promotable); }
    void setPromotable(Pi pi) { set(pi, Trait::Promotable); }
};

#endif
