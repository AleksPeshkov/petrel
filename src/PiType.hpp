#ifndef PI_TYPE_H
#define PI_TYPE_H

#include "typedefs.hpp"
#include "PiBit.hpp"

class PiType : protected PiBit<PiType, PieceType> {
    typedef PiBit<PiType, PieceType> Base;
    typedef PieceType::_t _t;

    enum : u8_t {
        QueenMask = ::singleton<u8_t>(Queen),
        RookMask = ::singleton<u8_t>(Rook),
        BishopMask = ::singleton<u8_t>(Bishop),
        KnightMask = ::singleton<u8_t>(Knight),
        PawnMask = ::singleton<u8_t>(Pawn),
        KingMask = ::singleton<u8_t>(King),
        SliderMask = QueenMask | RookMask | BishopMask,
        LeaperMask = KingMask | KnightMask | PawnMask,
    };

    static constexpr PieceType::arrayOf<u8_t> NotBadKillersMask = {
        PawnMask | KnightMask | BishopMask | RookMask | QueenMask, // Queen
        PawnMask | KnightMask | BishopMask | RookMask, // Rook
        PawnMask | KnightMask | BishopMask, // Bishop
        PawnMask | KnightMask | BishopMask, // Knight
        PawnMask, // Pawn
        0, // King
    };

    static constexpr PieceType::arrayOf<u8_t> GoodKillersMask = {
        PawnMask | KnightMask | BishopMask | RookMask, // Queen
        PawnMask | KnightMask | BishopMask, // Rook
        PawnMask, // Bishop
        PawnMask, // Knight
        0, // Pawn
        0, // King
    };

    bool is(Pi pi, PieceType ty) const { assertOk(pi); assert (!ty.is(King)); return Base::is(pi, ty); }

public:

#ifdef NDEBUG
    void assertOk(Pi) const {}
#else
    void assertOk(Pi pi) const {
        assert ( !isEmpty(pi) );
        assert ( ::isSingleton(get(pi)) );
    }
#endif

    PiMask pieces() const { return notEmpty(); }
    PiMask piecesOfType(PieceType ty) const { assert (!ty.is(King)); return anyOf(ty); }
    PiMask sliders() const { return anyOf(SliderMask); }
    PiMask leapers() const { return anyOf(LeaperMask); }

    // mask less valuable piece types
    PiMask goodKillers(PieceType ty) const { return anyOf(GoodKillersMask[ty]); }

    // mask of equal or less valuable types
    PiMask notBadKillers(PieceType ty) const { return anyOf(NotBadKillersMask[ty]); }

    PieceType typeOf(Pi pi) const { assertOk(pi); return static_cast<PieceType::_t>( ::lsb(static_cast<unsigned>(get(pi))) ); }

    bool isPawn(Pi pi) const { return is(pi, Pawn); }
    bool isRook(Pi pi) const { return is(pi, Rook); }
    bool isSlider(Pi pi) const { assertOk(pi); return (get(pi) & SliderMask) != 0; }

    void clear(Pi pi) { assertOk(pi); assert (pi != TheKing); assert (!Base::is(pi, King)); Base::clear(pi); }
    void drop(Pi pi, PieceType ty) { assert (isEmpty(pi)); assert (pi != TheKing || ty == King); set(pi, ty); }
    void promote(Pi pi, PromoType ty) { assert (isPawn(pi)); clear(pi); set(pi, ty); }
};

#endif
