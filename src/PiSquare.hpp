#ifndef PI_SQUARE_HPP
#define PI_SQUARE_HPP

#include "typedefs.hpp"
#include "Square.hpp"
#include "PiMask.hpp"
#include "PiVector.hpp"
#include "VectorOfAll.hpp"

class PiSquare : protected PiVector {
    enum { EmptySquare = 0xff };
    bool isEmpty(Pi pi) const { return PiVector::get(pi) == EmptySquare; }
    Square get(Pi pi) const { return static_cast<Square::_t>(PiVector::get(pi)); }

    PiMask piecesOn(Square sq) const { return PiMask::equals(v, ::vectorOfAll[sq]); }
    bool none(Square sq) { return piecesOn(sq).none(); }

#ifdef NDEBUG
    void assertOk(Pi) const {}
#else
    void assertOk(Pi pi) const {
        assert (!isEmpty(pi));
        assert (pieceOn(get(pi)) == pi);
    }
#endif

public:
    constexpr PiSquare () : PiVector(::vectorOfAll[EmptySquare]) {}

    PiMask pieces() const { return PiMask::cmpgt(v, ::vectorOfAll[EmptySquare]); }
    PiMask piecesOn(Rank rank) const { return PiMask::equals(
        v & ::vectorOfAll[0xff ^ File::Mask],
        ::vectorOfAll[static_cast<Square::_t>(rank << Square::RankShift)]);
    }

    Square squareOf(Pi pi) const { assertOk(pi); return get(pi); }
    bool has(Square sq) const { return piecesOn(sq).any(); }
    Pi pieceOn(Square sq) const { assert (has(sq)); return piecesOn(sq).index(); }

    void clear(Pi pi) { assertOk(pi); set(pi, EmptySquare); }
    void drop(Pi pi, Square sq) { assert (isEmpty(pi)); assert (none(sq)); set(pi, sq); }
    void move(Pi pi, Square sq) { assertOk(pi); assert (none(sq)); set(pi, sq); }

    void castle(Square kingTo, Pi theRook, Square rookTo) {
        assert (TheKing != theRook);
        assertOk(TheKing);
        assertOk(theRook);

        assert (squareOf(TheKing).on(Rank1));
        assert (squareOf(theRook).on(Rank1));

        assert (kingTo.is(G1) || kingTo.is(C1));
        assert (rookTo.is(F1) || rookTo.is(D1));

        set(TheKing, kingTo);
        set(theRook, rookTo);

        assertOk(TheKing);
        assertOk(theRook);
    }

};

#endif
