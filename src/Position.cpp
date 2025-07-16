#include "Position.hpp"
#include "CastlingRules.hpp"
#include "AttacksFrom.hpp"


void Position::makeMove(Square from, Square to) {
    PositionSide::swap(MY, OP);

    //the position just swapped its sides, so we make the move for the Op
    makeMove<Op>(from, to);

    //king cannot be left in check
    assert (MY.checkers().none());
}

void Position::makeMove(const Position& parent, Square from, Square to) {
    assert (this != &parent);
    MY = parent[Op];
    OP = parent[My];

    //current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op>(from, to);

    //king cannot be left in check
    assert (MY.checkers().none());
}

template <Side::_t My>
void Position::makeMove(Square from, Square to) {
    constexpr Side Op{~My};

    //Assumes that the given move is valid and legal
    OP.clearCheckers();
    assert (MY.checkers().none());

    Pi pi = MY.pieceAt(from);

    //clear en passant status from the previous move
    if (OP.hasEnPassant()) {
        MY.clearEnPassantKillers();

        //en passant capture encoded as the pawn captures the pawn
        if (MY.isPawn(pi) && from.on(Rank5) && to.on(Rank5)) {
            Square ep{File(to), Rank6};
            MY.movePawn(pi, from, ep);
            OP.capture(~to);
            updateSliderAttacks<My>(MY.attackersTo(from, to, ep), OP.attackersTo(~from, ~to, ~ep));
            return;
        }

        OP.clearEnPassantVictim();
    }
    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    if (MY.isPawn(pi)) { playPawnMove<My>(pi, from, to); return; }
    if (MY.kingSquare().is(from)) { playKingMove<My>(from, to); return; }

    //castling move encoded as castling rook captures own king
    if (MY.kingSquare().is(to)) { playCastling<My>(pi, from, to); return; }

    //simple non-pawn non-king move
    MY.move(pi, from, to);

    if (OP.has(~to)) {
        OP.capture(~to);
        updateSliderAttacks<My>(MY.attackersTo(from) | pi, OP.attackersTo(~from));
        return;
    }

    updateSliderAttacks<My>(MY.attackersTo(from, to), OP.attackersTo(~from, ~to));
}

template <Side::_t My>
void Position::playPawnMove(Pi pi, Square from, Square to) {
    constexpr Side Op{~My};

    if (from.on(Rank7)) {
        Square promotedTo{File(to), Rank8};

        MY.promote(pi, from, ::promoTypeFrom(Rank{to}), promotedTo);

        if (OP.has(~promotedTo)) {
            //promotion with capture
            OP.capture(~promotedTo);
            updateSliderAttacks<My>(MY.attackersTo(from) | pi, OP.attackersTo(~from));
            return;
        }

        //promotion without capture
        updateSliderAttacks<My>(MY.attackersTo(from, promotedTo) | pi, OP.attackersTo(~from, ~promotedTo));
        return;
    }

    MY.movePawn(pi, from, to);

    //possible en passant capture case already treated
    if (OP.has(~to)) {
        //simple pawn capture
        OP.capture(~to);
        updateSliderAttacks<My>(MY.attackersTo(from), OP.attackersTo(~from));
        return;
    }

    //simple pawn move
    updateSliderAttacks<My>(MY.attackersTo(from, to), OP.attackersTo(~from, ~to));

    if (from.on(Rank2) && to.on(Rank4)) {
        setLegalEnPassant<My>(pi, to);
    }
}

template <Side::_t My>
void Position::playKingMove(Square from, Square to) {
    constexpr Side Op{~My};

    MY.moveKing(from, to);
    OP.setOpKing(~to);

    if (OP.has(~to)) {
        //capture
        OP.capture(~to);
        updateSliderAttacks<My>(MY.attackersTo(from));
        return;
    }

    //non-capture
    updateSliderAttacks<My>(MY.attackersTo(from, to));
}

template <Side::_t My>
void Position::playCastling(Pi rook, Square rookFrom, Square kingFrom) {
    constexpr Side Op{~My};

    Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
    Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

    MY.castle(kingFrom, kingTo, rook, rookFrom, rookTo);
    OP.setOpKing(~kingTo);

    //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
    //TRICK: castling rook should attack 'kingFrom' square
    //TRICK: only first rank sliders can be affected
    updateSliderAttacks<My>(MY.attackersTo(rookFrom, kingFrom) & MY.piecesOn(Rank1));
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected) {
    constexpr Side Op{~My};

    PositionSide::syncOccupied(MY, OP);

    myAffected &= MY.sliders();
    if (myAffected.any()) {
        MY.updateSliderAttacks(myAffected);
    }
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected, PiMask opAffected) {
    constexpr Side Op{~My};

    updateSliderAttacks<My>(myAffected);

    opAffected &= OP.sliders();
    if (opAffected.any()) {
        OP.updateSliderAttacks(opAffected);
    }
}

template <Side::_t My>
void Position::setLegalEnPassant(Pi victim, Square to) {
    constexpr Side Op{~My};

    assert (MY.isPawn(victim));
    assert (MY.squareOf(victim).is(to));
    assert (to.on(Rank4));

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    Square ep{File(to), Rank3};

    // check if there are any pawns to capture ep victim
    Bb killers = ~OP.pawnsSquares() & ::attacksFrom(Pawn, ep);
    if (killers.none()) { return; }

    // discovered check
    if (MY.isPinned(MY.occupied())) { assert ((MY.checkers() % victim).any()); return; }
    assert ((MY.checkers() % victim).none());

    for (Square from : killers) {
        assert (from.on(Rank4));

        if (!MY.isPinned(MY.occupied() - from + ep - to)) {
            MY.setEnPassantVictim(victim);
            OP.setEnPassantKiller(OP.pieceAt(~from));
        }
    }
}

bool Position::dropValid(Side My, PieceType ty, Square to) {
    return MY.dropValid(ty, to);
}

bool Position::afterDrop() {
    PositionSide::finalSetup(MY, OP);
    updateSliderAttacks<Op>(OP.pieces(), MY.pieces());

    //should be not in check
    return MY.checkers().none();
}

bool Position::isSpecial(Square from, Square to) const {
    if (MY.kingSquare().is(to)) {
        return true; // castling
    }

    if (MY.isPawn(MY.pieceAt(from))) {
        if (from.on(Rank7)) {
            return true; // pawn promotion
        }
        if (from.on(Rank5) && to.on(Rank5)) {
            return true; // en passant
        }
    }

    return false; // normal move
}
