#ifndef PI_TYPE_H
#define PI_TYPE_H

#include "PiMask.hpp"

class PiType {
    enum pieces_t : u8_t {
        None   = 0,
        Queens  = ::singleton<u8_t>(Queen),
        Rooks   = ::singleton<u8_t>(Rook),
        Bishops = ::singleton<u8_t>(Bishop),
        Knights = ::singleton<u8_t>(Knight),
        Pawns   = ::singleton<u8_t>(Pawn),
        Kings   = ::singleton<u8_t>(King),
        Sliders  = Queens | Rooks | Bishops,
        Leapers  = Pawns | Knights | Kings,
        All      = Sliders | Leapers,
        Officers = Queens | Rooks | Bishops | Knights,
        NonPawns = Queens | Rooks | Bishops | Knights | Kings,
        NonKings = Queens | Rooks | Bishops | Knights | Pawns,
        PNBR     = Pawns | Knights | Bishops | Rooks,
        PNB      = Pawns | Knights | Bishops,
    };

    using element_type = pieces_t;

    static constexpr PieceType::arrayOf<element_type> LessOrEqualValue = {
        NonKings, // Queen
        PNBR,  // Rook
        PNB,   // Bishop
        PNB,   // Knight
        Pawns, // Pawn
        All,   // King
    };

    static constexpr PieceType::arrayOf<element_type> LessValue = {
        PNBR,  // Queen
        PNB,   // Rook
        Pawns, // Bishop
        Pawns, // Knight
        None,  // Pawn
        NonKings, // King
    };

    // defined to make debugging clear
    union {
        pieces_t type[16];
        vu8x16_t vu8x16;
    };

    constexpr element_type element(PieceType::_t ty) const { return static_cast<element_type>(::singleton<u8_t>(ty)); }
    constexpr vu8x16_t vector(element_type e) const { return ::vectorOfAll[static_cast<u8_t>(e)]; }
    constexpr vu8x16_t vector(PieceType::_t ty) const { return vector(element(ty)); }

    constexpr bool has(Pi pi, element_type e) const { assertOk(pi); return (static_cast<u8_t>(type[pi]) & static_cast<u8_t>(e)) != 0; }
    constexpr bool is(Pi pi, PieceType::_t ty) const { assertOk(pi);  return has(pi, element(ty)); }
    constexpr PiMask any(element_type e) const { return PiMask::any(vu8x16 & vector(e)); }

public:
    constexpr PiType () : type {
        None, None, None, None,
        None, None, None, None,
        None, None, None, None,
        None, None, None, None,
    } {}

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && ::isSingleton(static_cast<u8_t>(type[pi])); }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    void drop(Pi pi, PieceType::_t ty) { assert (isEmpty(pi)); assert (pi != TheKing || ty == King); type[pi] = element(ty); }
    void clear(Pi pi) { assertOk(pi); assert (pi != TheKing); assert (!is(pi, King)); type[pi] = None; }
    void promote(Pi pi, PromoType::_t ty) { assert (isPawn(pi)); type[pi] = element(ty); }

    constexpr bool isEmpty(Pi pi) const { return type[pi] == None; }
    constexpr bool isPawn(Pi pi) const { return is(pi, Pawn); }
    constexpr bool isRook(Pi pi) const { return is(pi, Rook); }
    constexpr bool isSlider(Pi pi) const { assertOk(pi); return has(pi, Sliders); }
    constexpr PieceType typeOf(Pi pi) const { assertOk(pi); return PieceType{static_cast<PieceType::_t>( ::lsb(static_cast<unsigned>(type[pi])) )}; }

    constexpr PiMask pieces() const { return PiMask::any(vu8x16); }
    constexpr PiMask piecesOfType(PieceType::_t ty) const { assert (!PieceType{ty}.is(King)); return any(element(ty)); }

    // Queens, Rooks, Bishops
    constexpr PiMask sliders() const { return any(Sliders); }

    // King, Pawns, Knights
    constexpr PiMask leapers() const { return any(Leapers); }

    // Queens, Rooks, Bishops, Knights
    constexpr PiMask officers() const { return any(Officers); }

    // King, Queens, Rooks, Bishops, Knights
    constexpr PiMask nonPawns() const { return any(NonPawns); }

    // Queens, Rooks, Bishops, Knights, Pawns
    constexpr PiMask nonKing() const { return any(NonKings); }

    // less valuable pieces than given piece type
    constexpr PiMask lessValue(PieceType ty) const {return any(LessValue[ty]); }

    // less or equal value pieces than given piece type
    constexpr PiMask lessOrEqualValue(PieceType ty) const { return any(LessOrEqualValue[ty]); }
};

#endif
