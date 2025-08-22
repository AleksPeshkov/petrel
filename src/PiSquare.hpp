#ifndef PI_SQUARE_HPP
#define PI_SQUARE_HPP

#include "PiMask.hpp"

enum class Sq : u8_t {
    A8, B8, C8, D8, E8, F8, G8, H8,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A1, B1, C1, D1, E1, F1, G1, H1,
    Empty = 0xff
};

class PiSquare {
    typedef Sq element_type;

    union {
        vu8x16_t vu8x16;
        Sq square[16];
    };

    constexpr void set(Pi pi, Square::_t sq) { square[pi] = static_cast<Sq>(sq); }
    constexpr vu8x16_t vector(element_type e) const { return ::vectorOfAll[static_cast<u8_t>(e)]; }
    constexpr vu8x16_t vector(Square::_t sq) const { return vector(static_cast<element_type>(sq)); }

public:
    constexpr PiSquare () : square {
        Sq::Empty,Sq::Empty,Sq::Empty,Sq::Empty,
        Sq::Empty,Sq::Empty,Sq::Empty,Sq::Empty,
        Sq::Empty,Sq::Empty,Sq::Empty,Sq::Empty,
        Sq::Empty,Sq::Empty,Sq::Empty,Sq::Empty,
    } {}

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && pieceAt(static_cast<Square::_t>(square[pi])) == pi; }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    void drop(Pi pi, Square sq) { assert (isEmpty(pi)); assert (!has(sq)); set(pi, sq); }
    constexpr void clear(Pi pi) { assertOk(pi); square[pi] = Sq::Empty; }
    void move(Pi pi, Square sq) { assertOk(pi); assert (!has(sq)); set(pi, sq); }

    constexpr void castle(Square kingTo, Pi theRook, Square rookTo) {
        assert (TheKing != theRook);
        assert (squareOf(TheKing).on(Rank1));
        assert (squareOf(theRook).on(Rank1));
        assert (kingTo.is(G1) || kingTo.is(C1));
        assert (rookTo.is(F1) || rookTo.is(D1));

        assertOk(TheKing);
        assertOk(theRook);
        set(TheKing, kingTo);
        set(theRook, rookTo);
        assertOk(TheKing);
        assertOk(theRook);
    }

    constexpr bool isEmpty(Pi pi) const { return square[pi] == Sq::Empty; }
    constexpr Square squareOf(Pi pi) const { assertOk(pi); return static_cast<Square::_t>(square[pi]); }

    bool has(Square sq) const { return piecesAt(sq).any(); }
    Pi pieceAt(Square sq) const { assert (has(sq)); return piecesAt(sq).index(); }

    PiMask pieces() const { return PiMask{vu8x16 != ::all(static_cast<u8_t>(Sq::Empty))}; }
    PiMask piecesAt(Square sq) const { return PiMask{vu8x16, vector(sq)}; }
    PiMask piecesOn(Rank rank) const { return PiMask{
        vu8x16 & vector(static_cast<Sq>(0xff ^ File::Mask)),
        vector(Square{static_cast<file_t>(0), rank})
    }; }
};

#endif
