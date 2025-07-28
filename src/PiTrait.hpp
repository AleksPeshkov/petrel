#ifndef PI_TRAIT_H
#define PI_TRAIT_H

#include "typedefs.hpp"

enum class Trait : u8_t {
    Empty      = 0,
    Checker    = ::singleton<u8_t>(0), // any piece actually attacking enemy king
    Pinner     = ::singleton<u8_t>(1), // potential pinner: sliding piece that can attack the enemy king square on empty board
    Castling   = ::singleton<u8_t>(2), // rook with castling rights
    EnPassant  = ::singleton<u8_t>(3), // pawn can be legally captured en passant OR pawn can perform a legal en passant capture
    Promotable = ::singleton<u8_t>(4), // any pawn on the 7th rank
    CheckerPinner = Pinner | Checker,
    CastlingPinner = Castling | Pinner,
    CastlingCheckerPinner = Castling | Pinner | Checker,
    CheckerEnPassant  = Checker | EnPassant,
    CheckerPromotable = Checker | Promotable,
};

class PiTrait {
    typedef Trait element_type;

    union {
        element_type trait[16];
        u8_t vu8x16 __attribute__((vector_size(16)));
    };

    PiMask any(element_type e) const { return PiMask::any(vu8x16 & ::vectorOfAll[static_cast<u8_t>(e)]); }
    constexpr void clear(element_type e) { vu8x16 &= ::vectorOfAll[0xff ^ static_cast<u8_t>(e)]; }

    constexpr bool has(Pi pi, element_type e) const {
        return (static_cast<u8_t>(trait[pi]) & static_cast<u8_t>(e)) != 0;
    }

    constexpr void add(Pi pi, element_type e) {
        trait[pi] = static_cast<element_type>(static_cast<u8_t>(trait[pi]) | static_cast<u8_t>(e));
    }

    constexpr void clear(Pi pi, element_type e) {
        trait[pi] = static_cast<element_type>(static_cast<u8_t>(trait[pi]) & (0xffu ^ static_cast<u8_t>(e)));
    }

public:
    constexpr PiTrait () : trait {
        Trait::Empty, Trait::Empty, Trait::Empty, Trait::Empty,
        Trait::Empty, Trait::Empty, Trait::Empty, Trait::Empty,
        Trait::Empty, Trait::Empty, Trait::Empty, Trait::Empty,
        Trait::Empty, Trait::Empty, Trait::Empty, Trait::Empty,
    } {}

    constexpr void clear(Pi pi) { trait[pi] = Trait::Empty; }
    constexpr bool isEmpty(Pi pi) const { return trait[pi] == Trait::Empty; }

    PiMask castlingRooks() const { return any(Trait::Castling); }
    constexpr bool isCastling(Pi pi) const { return has(pi, Trait::Castling); }
    constexpr void setCastling(Pi pi) { assert (!isCastling(pi)); add(pi, Trait::Castling); }
    constexpr void clearCastlings() { clear(Trait::Castling); }

    PiMask enPassantPawns() const { return any(Trait::EnPassant); }
    Pi getEnPassant() const { Pi pi = enPassantPawns().index(); return pi; }
    constexpr bool isEnPassant(Pi pi) const { return has(pi, Trait::EnPassant); }
    constexpr void setEnPassant(Pi pi) { add(pi, Trait::EnPassant); }
    constexpr void clearEnPassant(Pi pi) { assert (isEnPassant(pi)); clear(pi, Trait::EnPassant); }
    constexpr void clearEnPassants() { clear(Trait::EnPassant); }

    PiMask pinners() const { return any(Trait::Pinner); }
    constexpr bool isPinner(Pi pi) const { return has(pi, Trait::Pinner); }
    constexpr void clearPinners() { clear(Trait::Pinner); }
    constexpr void setPinner(Pi pi) { assert (!isPinner(pi)); add(pi, Trait::Pinner); }
    constexpr void clearPinner(Pi pi) { clear(pi, Trait::Pinner); }

    PiMask checkers() const { return any(Trait::Checker); }
    constexpr void clearCheckers() { clear(Trait::Checker); }
    constexpr void setChecker(Pi pi) { assert (!has(pi, Trait::Checker)); add(pi, Trait::Checker); }

    PiMask promotables() const { return any(Trait::Promotable); }
    constexpr bool isPromotable(Pi pi) const { return has(pi, Trait::Promotable); }
    constexpr void setPromotable(Pi pi) { add(pi, Trait::Promotable); }
};

#endif
