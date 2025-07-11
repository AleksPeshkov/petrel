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

    static constexpr PieceType::arrayOf<u8_t> GoodKillersMask = {
        PawnMask | KnightMask | BishopMask | RookMask | QueenMask, // Queen
        PawnMask | KnightMask | BishopMask | RookMask, // Rook
        PawnMask | KnightMask | BishopMask, // Bishop
        PawnMask | KnightMask | BishopMask, // Knight
        PawnMask, // Pawn
        PawnMask | KnightMask | BishopMask | RookMask | QueenMask, // King
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

    // mask of equal of less valuable piece
    PiMask goodKillers(PieceType ty) const { assert (!ty.is(King)); return anyOf(GoodKillersMask[ty]); }

    PieceType typeOf(Pi pi) const { assertOk(pi); return static_cast<PieceType::_t>( ::bsf(static_cast<unsigned>(get(pi))) ); }

    bool isPawn(Pi pi) const { return is(pi, Pawn); }
    bool isRook(Pi pi) const { return is(pi, Rook); }
    bool isSlider(Pi pi) const { assertOk(pi); return (get(pi) & SliderMask) != 0; }

    void clear(Pi pi) { assertOk(pi); assert (pi != TheKing); assert (!Base::is(pi, King)); Base::clear(pi); }
    void drop(Pi pi, PieceType ty) { assert (isEmpty(pi)); assert (pi != TheKing || ty == King); set(pi, ty); }
    void promote(Pi pi, PromoType ty) { assert (isPawn(pi)); clear(pi); set(pi, ty); }
};

#endif
