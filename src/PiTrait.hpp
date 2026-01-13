#ifndef PI_TRAIT_H
#define PI_TRAIT_H

#include "PiMask.hpp"

class PiTrait {
    enum trait_t : u8_t {
        Empty       = 0,
        Checkers    = ::singleton<u8_t>(0), // any piece actually attacking enemy king
        Pinners     = ::singleton<u8_t>(1), // potential pinner: sliding piece that can attack the enemy king square on empty board
        Castlings   = ::singleton<u8_t>(2), // rook with castling rights
        EnPassants  = ::singleton<u8_t>(3), // pawn can be legally captured en passant OR pawn can perform a legal en passant capture
        Promotables = ::singleton<u8_t>(4), // any pawn on the 7th rank
        CheckersPinners = Checkers | Pinners,  // Checkers + Pinners
    };

    using element_type = trait_t;

    // defined to make debugging clear
    union {
        element_type trait[16];
        vu8x16_t vu8x16;
    };

    constexpr PiMask any(element_type e) const { return PiMask::any(vu8x16 & ::vectorOfAll[static_cast<u8_t>(e)]); }
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
        Empty, Empty, Empty, Empty,
        Empty, Empty, Empty, Empty,
        Empty, Empty, Empty, Empty,
        Empty, Empty, Empty, Empty,
    } {}

    constexpr void clear(Pi pi) { trait[pi] = Empty; }
    constexpr bool isEmpty(Pi pi) const { return trait[pi] == Empty; }

    constexpr PiMask castlingRooks() const { return any(Castlings); }
    constexpr bool isCastling(Pi pi) const { return has(pi, Castlings); }
    constexpr void setCastling(Pi pi) { assert (!isCastling(pi)); add(pi, Castlings); }
    constexpr void clearCastlings() { clear(Castlings); }

    constexpr PiMask enPassantPawns() const { return any(EnPassants); }
    constexpr Pi getEnPassant() const { Pi pi = enPassantPawns().index(); return pi; }
    constexpr bool isEnPassant(Pi pi) const { return has(pi, EnPassants); }
    constexpr void setEnPassant(Pi pi) { add(pi, EnPassants); }
    constexpr void clearEnPassant(Pi pi) { assert (isEnPassant(pi)); clear(pi, EnPassants); }
    constexpr void clearEnPassants() { clear(EnPassants); }

    constexpr PiMask pinners() const { return any(Pinners); }
    constexpr bool isPinner(Pi pi) const { return has(pi, Pinners); }
    constexpr void clearPinners() { clear(Pinners); }
    constexpr void setPinner(Pi pi) { assert (!isPinner(pi)); add(pi, Pinners); }
    constexpr void clearPinner(Pi pi) { clear(pi, Pinners); }

    constexpr PiMask checkers() const { return any(Checkers); }
    constexpr void clearCheckers() { clear(Checkers); }
    constexpr void setChecker(Pi pi) { assert (!has(pi, Checkers)); add(pi, Checkers); }

    constexpr PiMask promotables() const { return any(Promotables); }
    constexpr bool isPromotable(Pi pi) const { return has(pi, Promotables); }
    constexpr void setPromotable(Pi pi) { add(pi, Promotables); }
};

#endif
