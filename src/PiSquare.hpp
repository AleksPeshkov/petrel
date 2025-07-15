#ifndef PI_SQUARE_HPP
#define PI_SQUARE_HPP

#include "typedefs.hpp"
#include "PiMask.hpp"
#include "VectorOfAll.hpp"

class PiSquare {
    typedef u8x16_t _t;
    _t v;

    enum { NoSquare = 0xff };
    bool isEmpty(Pi pi) const { return v.u8[pi] == NoSquare; }

    void set(Pi pi, Square sq) { v.u8[pi] = sq; }
    Square get(Pi pi) const { return static_cast<Square::_t>(v.u8[pi]); }

    PiMask piecesAt(Square sq) const { return PiMask::equals(v, ::vectorOfAll[sq]); }
    bool none(Square sq) { return piecesAt(sq).none(); }

#ifdef NDEBUG
    void assertOk(Pi) const {}
#else
    void assertOk(Pi pi) const {
        assert (!isEmpty(pi));
        assert (pieceAt(get(pi)) == pi);
    }
#endif

public:
    constexpr PiSquare () : v{::vectorOfAll[NoSquare]} {}

    PiMask pieces() const { return PiMask::cmpgt(v, ::vectorOfAll[NoSquare]); }
    PiMask piecesOn(Rank rank) const { return PiMask::equals(
        v.i128 & ::vectorOfAll[0xff ^ File::Mask].i128,
        ::vectorOfAll[static_cast<Square::_t>(rank << Square::RankShift)]);
    }

    Square squareOf(Pi pi) const { assertOk(pi); return get(pi); }
    bool has(Square sq) const { return piecesAt(sq).any(); }
    Pi pieceAt(Square sq) const { assert (has(sq)); return piecesAt(sq).index(); }

    void clear(Pi pi) { assertOk(pi); v.u8[pi] = NoSquare; }
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
