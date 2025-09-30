#ifndef PI_TYPE_H
#define PI_TYPE_H

#include "PiMask.hpp"

enum class Type : u8_t {
    Empty   = 0,
    Queen  = ::singleton<u8_t>(piece_type_t::Queen),
    Rook   = ::singleton<u8_t>(piece_type_t::Rook),
    Bishop = ::singleton<u8_t>(piece_type_t::Bishop),
    Knight = ::singleton<u8_t>(piece_type_t::Knight),
    Pawn   = ::singleton<u8_t>(piece_type_t::Pawn),
    King   = ::singleton<u8_t>(piece_type_t::King),
    Slider = Queen | Rook | Bishop,
    Leaper = Pawn | Knight | King,
    PNBRQ  = Pawn | Knight | Bishop | Rook | Queen,
    PNBR   = Pawn | Knight | Bishop | Rook,
    PNB    = Pawn | Knight | Bishop,
};

class PiType {
    using element_type = Type;

    static constexpr PieceType::arrayOf<element_type> LessOrEqualValue = {
        Type::PNBRQ, // Queen
        Type::PNBR,  // Rook
        Type::PNB,   // Bishop
        Type::PNB,   // Knight
        Type::Pawn,  // Pawn
        Type::Empty, // King
    };

    static constexpr PieceType::arrayOf<element_type> LessValue = {
        Type::PNBR,  // Queen
        Type::PNB,   // Rook
        Type::Pawn,  // Bishop
        Type::Pawn,  // Knight
        Type::Empty, // Pawn
        Type::Empty, // King
    };

    union {
        Type type[16];
        u8_t vu8x16 __attribute__((vector_size(16)));
    };

    constexpr element_type element(PieceType::_t ty) const { return static_cast<element_type>(::singleton<u8_t>(ty)); }
    constexpr vu8x16_t vector(element_type e) const { return ::vectorOfAll[static_cast<u8_t>(e)]; }
    constexpr vu8x16_t vector(PieceType::_t ty) const { return vector(element(ty)); }

    constexpr bool has(Pi pi, element_type e) const { assertOk(pi); return (static_cast<u8_t>(type[pi]) & static_cast<u8_t>(e)) != 0; }
    constexpr bool is(Pi pi, PieceType::_t ty) const { assertOk(pi);  return has(pi, element(ty)); }
    PiMask any(element_type e) const { return PiMask::any(vu8x16 & vector(e)); }

public:
    constexpr PiType () : type {
        Type::Empty, Type::Empty, Type::Empty, Type::Empty,
        Type::Empty, Type::Empty, Type::Empty, Type::Empty,
        Type::Empty, Type::Empty, Type::Empty, Type::Empty,
        Type::Empty, Type::Empty, Type::Empty, Type::Empty,
    } {}

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && ::isSingleton(static_cast<u8_t>(type[pi])); }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    constexpr void drop(Pi pi, PieceType::_t ty) { assert (isEmpty(pi)); assert (pi != TheKing || ty == King); type[pi] = element(ty); }
    constexpr void clear(Pi pi) { assertOk(pi); assert (pi != TheKing); assert (!is(pi, King)); type[pi] = Type::Empty; }
    constexpr void promote(Pi pi, PromoType::_t ty) { assert (isPawn(pi)); type[pi] = element(ty); }

    constexpr bool isEmpty(Pi pi) const { return type[pi] == Type::Empty; }
    constexpr bool isPawn(Pi pi) const { return is(pi, Pawn); }
    constexpr bool isRook(Pi pi) const { return is(pi, Rook); }
    constexpr bool isSlider(Pi pi) const { assertOk(pi); return has(pi, Type::Slider); }
    PieceType typeOf(Pi pi) const { assertOk(pi); return PieceType{static_cast<PieceType::_t>( ::lsb(static_cast<unsigned>(type[pi])) )}; }

    PiMask pieces() const { return PiMask::any(vu8x16); }
    PiMask piecesOfType(PieceType::_t ty) const { assert (!PieceType{ty}.is(King)); return any(element(ty)); }
    PiMask sliders() const { return any(Type::Slider); }
    PiMask leapers() const { return any(Type::Leaper); }

    // mask of less valuable piece types
    PiMask lessValue(PieceType ty) const {return any(LessValue[ty]); }
    bool isLessValue(Pi attacker, PieceType victim) const { return has(attacker, LessValue[victim]); }

    // mask of equal or less valuable types
    PiMask lessOrEqualValue(PieceType ty) const { return any(LessOrEqualValue[ty]); }
    bool isLessOrEqualValue(Pi attacker, PieceType victim) const { return has(attacker, LessOrEqualValue[victim]); }
};

#endif
