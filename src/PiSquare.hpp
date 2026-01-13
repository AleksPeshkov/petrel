#ifndef PI_SQUARE_HPP
#define PI_SQUARE_HPP

#include "Index.hpp"
#include "PiMask.hpp"

class PiSquare {
    using _t = Square::_t;
    constexpr static _t NoSq = static_cast<_t>(0xff); // captured piece

    // defined to make debugging clear
    union {
        vu8x16_t vu8x16;
        Pi::arrayOf<_t> square;
    };

    constexpr void set(Pi pi, _t sq) { square[pi] = sq; }
    constexpr vu8x16_t vector(_t e) const { return ::vectorOfAll[e]; }

public:
    constexpr PiSquare (): square {
        NoSq,NoSq,NoSq,NoSq,
        NoSq,NoSq,NoSq,NoSq,
        NoSq,NoSq,NoSq,NoSq,
        NoSq,NoSq,NoSq,NoSq,
    } {}

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && pieceAt(square[pi]) == pi; }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    void drop(Pi pi, Square sq) { assert (isEmpty(pi)); assert (!has(sq)); set(pi, sq); }
    void clear(Pi pi) { assertOk(pi); square[pi] = NoSq; }
    void move(Pi pi, Square sq) { assertOk(pi); assert (!has(sq)); set(pi, sq); }

    void castle(Square kingTo, Pi theRook, Square rookTo) {
        assert (TheKing != theRook);
        assert (squareOf(Pi{TheKing}).on(Rank1));
        assert (squareOf(theRook).on(Rank1));
        assert (kingTo.is(G1) || kingTo.is(C1));
        assert (rookTo.is(F1) || rookTo.is(D1));

        assertOk(Pi{TheKing});
        assertOk(theRook);
        set(Pi{TheKing}, kingTo);
        set(theRook, rookTo);
        assertOk(Pi{TheKing});
        assertOk(theRook);
    }

    constexpr bool isEmpty(Pi pi) const { return square[pi] == NoSq; }
    constexpr Square squareOf(Pi pi) const { assertOk(pi); return Square{square[pi]}; }

    constexpr bool has(_t sq) const { return piecesAt(sq).any(); }
    constexpr Pi pieceAt(_t sq) const { assert (has(sq)); return piecesAt(sq).index(); }

    constexpr PiMask pieces() const { return PiMask::notEquals(vu8x16, ::all(NoSq)); }
    constexpr PiMask piecesAt(_t sq) const { return PiMask::equals(vu8x16, vector(sq)); }

    constexpr PiMask piecesOn(Rank::_t rank) const {
        return PiMask::equals(
            vu8x16 & vector(static_cast<_t>(NoSq ^ static_cast<_t>(File::Mask))),
            vector(Square{static_cast<File::_t>(0), rank})
        );
    }
};

#endif
