#ifndef PI_SQUARE_HPP
#define PI_SQUARE_HPP

#include "typedefs.hpp"
#include "PiMask.hpp"
#include "VectorOfAll.hpp"

class PiSquare {
public:
    typedef u8x16_t _t;
    enum { NoSquare = 0xff };

protected:
    _t v;
    void set(Pi pi, Square sq) { v.u8[pi] = sq; }

public:
    constexpr PiSquare () : v{::vectorOfAll[NoSquare]} {}

    bool isOk(Pi pi) const { return !isEmpty(pi) && pieceAt(static_cast<Square::_t>(v.u8[pi])) == pi; }

#ifdef NDEBUG
    void assertOk(Pi) const {}
#else
    void assertOk(Pi pi) const { assert (isOk(pi)); }
#endif

    PiMask pieces() const { return PiMask::cmpgt(v, ::vectorOfAll[NoSquare]); }
    PiMask piecesOn(Rank rank) const { return PiMask::equals(
        static_cast<i128_t>(v) & static_cast<i128_t>(::vectorOfAll[0xff ^ File::Mask]),
        ::vectorOfAll[Square{static_cast<file_t>(0), rank}]);
    }
    PiMask piecesAt(Square sq) const { return PiMask::equals(v, ::vectorOfAll[sq]); }
    bool has(Square sq) const { return piecesAt(sq).any(); }
    Pi pieceAt(Square sq) const { assert (has(sq)); return piecesAt(sq).index(); }

    bool isEmpty(Pi pi) const { return v.u8[pi] == NoSquare; }
    Square squareOf(Pi pi) const { assertOk(pi); return static_cast<Square::_t>(v.u8[pi]); }

    void clear(Pi pi) { assertOk(pi); v.u8[pi] = NoSquare; }
    void drop(Pi pi, Square sq) { assert (isEmpty(pi)); assert (!has(sq)); set(pi, sq); }
    void move(Pi pi, Square sq) { assertOk(pi); assert (!has(sq)); set(pi, sq); }

    void castle(Square kingTo, Pi theRook, Square rookTo) {
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

};

#endif
