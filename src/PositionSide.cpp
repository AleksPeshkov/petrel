#include "PositionSide.hpp"
#include "Hyperbola.hpp"
#include "CastlingRules.hpp"
#include "AttacksFrom.hpp"

#ifndef NDEBUG
    void PositionSide::assertOk(Pi pi) const {
        types.assertOk(pi);

        Square sq = squares.squareOf(pi);
        assert (has(sq));

        assert (types.isPawn(pi) == pawnsBb.has(sq));
        assert (!types.isPawn(pi) || (!sq.on(Rank1) && !sq.on(Rank8)));

        assert (traits.isPromotable(pi) == (types.isPawn(pi) && sq.on(Rank7)));
        assert (traits.isEnPassant(pi) <= ( types.isPawn(pi) && (sq.on(Rank4) || sq.on(Rank5)) ));
        assert (traits.isCastling(pi)  <= (types.isRook(pi) && sq.on(Rank1)) );
    }

    void PositionSide::assertOk(Pi pi, PieceType ty, Square sq) const {
        assertOk(pi);
        assert (squares.squareOf(pi) == sq);
        assert (types.typeOf(pi) == ty);
    }
#endif

void PositionSide::swap(PositionSide& MY, PositionSide& OP) {
    using std::swap;
    swap(MY.attacks, OP.attacks);
    swap(MY.types, OP.types);
    swap(MY.traits, OP.traits);
    swap(MY.squares, OP.squares);
    swap(MY.piecesBb, OP.piecesBb);
    swap(MY.pawnsBb, OP.pawnsBb);
    swap(MY.evaluation, OP.evaluation);
    swap(MY.opKing, OP.opKing);
}

void PositionSide::finalSetup(PositionSide& MY, PositionSide& OP) {
    MY.setOpKing(~OP.kingSquare());
    OP.setOpKing(~MY.kingSquare());

    MY.setLeaperAttacks();
    OP.setLeaperAttacks();
}

void PositionSide::setLeaperAttacks() {
    assert (traits.checkers().none());

    for (Pi pi : types.leapers()) {
        setLeaperAttack(pi, typeOf(pi), squareOf(pi));
    }
}

Score PositionSide::evaluate(const PositionSide& MY, const PositionSide& OP) {
    return Evaluation::evaluate(MY.evaluation, OP.evaluation);
}

void PositionSide::capture(Square from) {
    Pi pi = pieceAt(from);
    PieceType ty = typeOf(pi);
    assert (!ty.is(King));

    assertOk(pi, ty, from);

    piecesBb -= from;
    pawnsBb &= piecesBb; //clear if pawn

    evaluation.capture(ty, from);
    attacks.clear(pi);
    squares.clear(pi);
    types.clear(pi);
    traits.clear(pi);
}

void PositionSide::move(Pi pi, PieceType ty, Square from, Square to) {
    assert (from != to);
    assertOk(pi, ty, from);

    squares.move(pi, to);
    piecesBb.move(from, to);
    evaluation.move(ty, from, to);
}

// simple non king, non pawn move
void PositionSide::move(Pi pi, Square from, Square to) {
    PieceType ty = typeOf(pi);
    assert (!ty.is(King));
    assert (!ty.is(Pawn));

    move(pi, ty, from, to);

    if (ty.is(Knight)) {
        assert (traits.isEmpty(pi)); //nothing to clear or already cleared
        setLeaperAttack(pi, Knight, to);
    }
    else {
        traits.clear(pi);
        setPinner(pi, ty, to);
    }

    assertOk(pi, ty, to);
}

void PositionSide::moveKing(Square from, Square to) {
    move(TheKing, King, from, to);
    updateMovedKing(to);
}

void PositionSide::movePawn(Pi pi, Square from, Square to) {
    move(pi, Pawn, from, to);
    pawnsBb.move(from, to);

    assert (traits.isEmpty(pi));
    if (to.on(Rank7)) { traits.setPromotable(pi); }

    setLeaperAttack(pi, Pawn, to);

    assertOk(pi, Pawn, to);
}

void PositionSide::promote(Pi pi, Square from, PromoType ty, Square to) {
    assert (from.on(Rank7));
    assert (to.on(Rank8));
    assertOk(pi, Pawn, from);

    squares.move(pi, to);
    piecesBb.move(from, to);
    pawnsBb -= from;

    types.promote(pi, ty);
    evaluation.promote(from, to, ty);

    assert (traits.isPromotable(pi));
    traits.clear(pi);

    if (ty.is(Knight)) {
        setLeaperAttack(pi, Knight, to);
    }
    else {
        setPinner(pi, PieceType{ty}, to);
    }

    assertOk(pi, PieceType{ty}, to);
}

void PositionSide::updateMovedKing(Square to) {
    //king move cannot check
    assert (traits.isEmpty(TheKing));
    assert (!::attacksFrom(King, to).has(opKing));
    attacks.set(TheKing, ::attacksFrom(King, to));
    traits.clearCastlings();

    assertOk(TheKing, King, to);
}

void PositionSide::castle(Square kingFrom, Square kingTo, Pi rook, Square rookFrom, Square rookTo) {
    assertOk(TheKing, King, kingFrom);
    assertOk(rook, Rook, rookFrom);

    //possible overlap in Chess960
    squares.castle(kingTo, rook, rookTo);
    piecesBb -= kingFrom;
    piecesBb -= rookFrom;
    piecesBb += kingTo;
    piecesBb += rookTo;

    evaluation.castle(kingFrom, kingTo, rookFrom, rookTo);

    traits.clearPinner(rook);
    setPinner(rook, Rook, rookTo);

    updateMovedKing(kingTo);
    assertOk(rook, Rook, rookTo);
}

void PositionSide::setLeaperAttack(Pi pi, PieceType ty, Square sq) {
    assertOk(pi, ty, sq);
    assert (isLeaper(ty));
    assert (traits.isEmpty(pi) || traits.isPromotable(pi));

    attacks.set(pi, ::attacksFrom(ty, sq));
    if (::attacksFrom(ty, sq).has(opKing)) {
        traits.setChecker(pi);
    }
}

void PositionSide::setPinner(Pi pi, PieceType ty, Square sq) {
    assert (isSlider(ty));
    assert (!traits.isPinner(pi));

    if (::attacksFrom(ty, sq).has(opKing) && ::inBetween(opKing, sq).any()) {
        traits.setPinner(pi);
    }
}

void PositionSide::setOpKing(Square king) {
    opKing = king;

    assert (traits.checkers().none()); //king should not be in check

    traits.clearPinners();
    for (Pi pi : types.sliders()) {
        Square sq = squareOf(pi);
        if (::attacksFrom(typeOf(pi), sq).has(opKing)) {
            traits.setPinner(pi);
        }
    }
}

void PositionSide::updateSliders(PiMask affectedSliders, Bb occupiedBb) {
    assert (traits.checkers().none());
    assert (affectedSliders.any());

    Hyperbola blockers{ occupiedBb };

    for (Pi pi : affectedSliders) {
        Bb attack = blockers.attack(SliderType{typeOf(pi)}, squareOf(pi));
        attacks.set(pi, attack);

        assert (!attack.has(opKing)); // king cannot be left in check
    }
}

void PositionSide::updateSlidersCheckers(PiMask affectedSliders, Bb occupiedBb) {
    assert ((traits.checkers() & types.sliders()).none());
    assert (affectedSliders.any());

    //TRICK: attacks calculated without opponent's king for implicit out of check king moves generation
    Hyperbola blockers{ occupiedBb - opKing };

    for (Pi pi : affectedSliders) {
        Bb attack = blockers.attack(SliderType{typeOf(pi)}, squareOf(pi));
        attacks.set(pi, attack);

        if (attack.has(opKing)) {
            traits.setChecker(pi);
        }
    }
}

void PositionSide::setEnPassantVictim(Pi pi) {
    assert (isPawn(pi));
    assert (squareOf(pi).on(Rank4));
    assert (!hasEnPassant() || traits.isEnPassant(pi));
    traits.setEnPassant(pi);
}

void PositionSide::setEnPassantKiller(Pi pi) {
    assert (isPawn(pi));
    assert (squareOf(pi).on(Rank5));
    traits.setEnPassant(pi);
}

void PositionSide::clearEnPassantVictim() {
    assert (hasEnPassant());
    assert (traits.enPassantPawns().isSingleton());
    assert (traits.enPassantPawns() <= squares.piecesOn(Rank4));
    traits.clearEnPassants();
}

void PositionSide::clearEnPassantKillers() {
    assert (hasEnPassant());
    assert (traits.enPassantPawns() <= squares.piecesOn(Rank5));
    traits.clearEnPassants();
}

bool PositionSide::isPinned(Bb occupied) const {
    for (Pi pinner : pinners()) {
        Bb pinLine = ::inBetween(opKing, squareOf(pinner));
        if ((pinLine & occupied).none()) {
            return true;
        }
    }

    return false;
}

bool PositionSide::dropValid(PieceType ty, Square to) {
    if (piecesBb.has(to)) { return false; }
    piecesBb += to;

    Pi pi = ty.is(King) ? Pi{TheKing} : (pieces() | Pi{TheKing}).seekVacant();

    evaluation.drop(ty, to);
    types.drop(pi, ty);
    squares.drop(pi, to);

    if (ty.is(Pawn)) {
        if (to.on(Rank1) || to.on(Rank8)) { return false; }
        if (to.on(Rank7)) { traits.setPromotable(pi);}
        pawnsBb += to;
    }

    assertOk(pi, ty, to);
    return true;
}

bool PositionSide::setValidCastling(CastlingSide castlingSide) {
    if (!kingSquare().on(Rank1)) { return false; }

    Square outerSquare = kingSquare();
    for (Pi rook : piecesOfType(Rook) & piecesOn(Rank1)) {
        if (CastlingRules::castlingSide(outerSquare, squareOf(rook)).is(castlingSide)) {
            outerSquare = squareOf(rook);
        }
    }
    if (outerSquare == kingSquare()) { return false; } //no rook found

    Pi rook = pieceAt(outerSquare);
    if (isCastling(rook)) { return false; }

    traits.setCastling(rook);
    return true;
}

bool PositionSide::setValidCastling(File file) {
    if (!kingSquare().on(Rank1)) { return false; }

    Square rookFrom(file, Rank1);
    if (!has(rookFrom)) { return false; }

    Pi rook = pieceAt(rookFrom);
    if (!types.isRook(rook)) { return false; }
    if (isCastling(rook)) { return false; }

    traits.setCastling(rook);
    return true;
}
