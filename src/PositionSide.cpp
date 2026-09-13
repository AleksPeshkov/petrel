#include "PositionSide.hpp"
#include "Hyperbola.hpp"

#ifndef NDEBUG
#endif

void PositionSide::swap(PositionSide& MY, PositionSide& OP) {
    using std::swap;
    swap(MY.attacks_, OP.attacks_);
    swap(MY.types, OP.types);
    swap(MY.traits, OP.traits);
    swap(MY.squares, OP.squares);
    swap(MY.bbSide_, OP.bbSide_);
    swap(MY.bbPawns_, OP.bbPawns_);
    swap(MY.bbPawnAttacks_, OP.bbPawnAttacks_);
    swap(MY.material_, OP.material_);
    swap(MY.opKing, OP.opKing);
}

void PositionSide::finalSetup(PositionSide& MY, PositionSide& OP) {
    MY.setOpKing(~OP.sqKing());
    OP.setOpKing(~MY.sqKing());

    MY.setLeaperAttacks();
    OP.setLeaperAttacks();
}

int PositionSide::countAttackersTo(Square sq0, Bb occupied) const {
    auto attackers = attackersTo(sq0);

    int count = attackers.popcount();
    if (count == 0) { return count; }

    auto sliders1 = sliders() & attackers; // primary slider attackers
    if (sliders1.isNone()) { return count; }

    auto sliders2 = sliders() % attackers; // secondary slider attackers
    if (sliders2.isNone()) { return count; }

    for (auto pi2 : sliders2) {
        Square sq2{sq(pi2)};
        if (!::attacksFrom(piece(pi2), sq0).has(sq2)) {
            sliders2 -= PiMask{pi2};
        }
    }
    if (sliders2.isNone()) { return count; }

    for (auto pi1 : sliders1) {
        Square sq1{sq(pi1)};

        Bb candidates{};
        for (auto pi2 : sliders2) {
            Square sq2{sq(pi2)};
            if (::inBetween(sq0, sq2).has(sq1)) {
                candidates += Bb{sq2};
                sliders2 -= PiMask{pi2};
            }
        }

        // triple battery possible
        for (auto sq2 : candidates) {
            if ((::inBetween(sq2, sq1) & (occupied - candidates)).isNone()) {
                ++count;
            }
        }
    }

    return count;
}

void PositionSide::setLeaperAttacks() {
    assert (traits.checkers().isNone());

    for (Pi pi : types.leapers()) {
        setLeaperAttack(pi, piece(pi), sq(pi));
    }
}

void PositionSide::capture(Square from) {
    Pi pi{ this->pi(from) };
    NonKingPiece nonKing{*piece(pi)};
    assert (!nonKing.is(King));

    assertOk(pi, nonKing, from);

    bbSide_ -= Bb{from};
    if (nonKing.is(Pawn)) {
        bbPawns_ -= Bb{from};
        bbPawnAttacks_ = bbPawns_.forwardDiag();
    }

    material_.clear(nonKing);
    attacks_.clear(pi);
    squares.clear(pi);
    types.clear(pi);
    traits.clear(pi);
}

void PositionSide::move(Pi pi, Square from, Square to) {
    assert (from != to);
    assertOk(pi, piece(pi), from);

    squares.set(pi, to);
    bbSide_.move(from, to);
}

// simple non king, non pawn move
void PositionSide::move(Pi pi, Officer officer, Square from, Square to) {
    move(pi, from, to);

    if (officer.is(Knight)) {
        assert (traits.isNone(pi)); // nothing to clear or already cleared
        setLeaperAttack(pi, Knight, to);
    }
    else {
        traits.clear(pi);
        setPinner(pi, Slider{*officer}, to);
    }

    assertOk(pi, officer, to);
}

void PositionSide::movePawn(Square from, Square to) {
    Pi pawn{pi(from)};
    move(pawn, from, to);
    bbPawns_.move(from, to);
    bbPawnAttacks_ = bbPawns_.forwardDiag();

    assert (traits.isNone(pawn));
    if (to.isOn(Rank7)) { traits.setPromotable(pawn); }

    setLeaperAttack(pawn, Pawn, to);

    assertOk(pawn, Pawn, to);
}

Pi PositionSide::piPromoted(Square from, Officer officer, Square to) {
    Pi pawn{ pi(from) };
    assert (from.isOn(Rank7));
    assert (to.isOn(Rank8));
    assert (traits.isPromotable(pawn));
    assertOk(pawn, Pawn, from);

    bbSide_.move(from, to);
    material_.promote(officer);

    // remove pawn
    bbPawns_ -= Bb{from};
    bbPawnAttacks_ = bbPawns_.forwardDiag();
    attacks_.clear(pawn);
    squares.clear(pawn);
    traits.clear(pawn);
    types.clear(pawn);

    // drop promoted piece to the most valuable if possible
    //TODO: resort all pieces
    Pi promoted{ PieceSet(any()).piFirstVacant() };
    assert (promoted <= pawn);

    squares.drop(promoted, to);
    types.drop(promoted, officer);

    if (officer.is(Knight)) {
        setLeaperAttack(promoted, Knight, to);
    }
    else {
        setPinner(promoted, Slider{*officer}, to);
    }

    assertOk(promoted, officer, to);
    return promoted;
}

void PositionSide::updateMovedKing(Square to) {
    // king move cannot check
    assert (traits.isNone(PiKing));
    assert (!::attacksFrom(King, to).has(opKing));
    attacks_.set(PiKing, ::attacksFrom(King, to));
    traits.clearCastlings();

    assertOk(PiKing, King, to);
}

void PositionSide::castle(Square kingFrom, Square kingTo, Pi piRook, Square rookFrom, Square rookTo) {
    assertOk(PiKing, King, kingFrom);
    assertOk(piRook, Rook, rookFrom);

    // possible overlap in Chess960
    squares.castle(kingTo, piRook, rookTo);
    bbSide_ -= Bb{kingFrom};
    bbSide_ -= Bb{rookFrom};
    bbSide_ += Bb{kingTo};
    bbSide_ += Bb{rookTo};

    traits.clearPinner(piRook);
    setPinner(piRook, Rook, rookTo);

    updateMovedKing(kingTo);
    assertOk(piRook, Rook, rookTo);
}

void PositionSide::setLeaperAttack(Pi pi, Piece piece, Square sq) {
    assertOk(pi, piece, sq);
    assert (::isLeaper(*piece));
    assert (traits.isNone(pi) || traits.isPromotable(pi));

    attacks_.set(pi, ::attacksFrom(piece, sq));
    if (::attacksFrom(piece, sq).has(opKing)) {
        traits.setChecker(pi);
    }
}

void PositionSide::setPinner(Pi pi, Slider slider, Square sq) {
    assert (!traits.isPinner(pi));

    if (::attacksFrom(slider, sq).has(opKing) && ::inBetween(opKing, sq).isAny()) {
        traits.setPinner(pi);
    }
}

void PositionSide::setOpKing(Square king) {
    opKing = king;

    assert (traits.checkers().isNone()); // king should not be in check

    traits.clearPinners();
    for (Pi pi : sliders()) {
        if (::attacksFrom(piece(pi), sq(pi)).has(opKing)) {
            traits.setPinner(pi);
        }
    }
}

void PositionSide::updateSliders(PiMask affectedSliders, Bb occupiedBb) {
    assert (traits.checkers().isNone());
    assert (affectedSliders.isAny());

    Hyperbola blockers{ occupiedBb };

    for (Pi pi : affectedSliders) {
        Bb attack = blockers.attack(Slider{*piece(pi)}, sq(pi));
        attacks_.set(pi, attack);

        assert (!attack.has(opKing)); // king cannot be left in check
    }
}

void PositionSide::updateSlidersCheckers(PiMask affectedSliders, Bb occupiedBb) {
    assert (sliders().isNone(traits.checkers()));
    assert (affectedSliders.isAny());

    //TRICK: attacks calculated without opponent's king for implicit out of check king moves generation
    Hyperbola blockers{ occupiedBb - Bb{opKing} };

    for (Pi pi : affectedSliders) {
        Bb attack = blockers.attack(Slider{*piece(pi)}, sq(pi));
        attacks_.set(pi, attack);

        if (attack.has(opKing)) {
            traits.setChecker(pi);
        }
    }
}

void PositionSide::setEnPassantVictim(Square ep) {
    assert (isPawn(ep));
    assert (ep.isOn(Rank4));
    assert (!hasEnPassant() || traits.isEnPassant(pi(ep)));
    traits.setEnPassant(pi(ep));
}

void PositionSide::setEnPassantKiller(Square from) {
    assert (isPawn(from));
    assert (from.isOn(Rank5));
    traits.setEnPassant(pi(from));
}

void PositionSide::clearEnPassantVictim() {
    assert (hasEnPassant());
    assert (traits.enPassantPawns().isSingleton());
    assert (traits.enPassantPawns() <= squares.any(Rank4));
    traits.clearEnPassants();
}

void PositionSide::clearEnPassantKillers() {
    assert (hasEnPassant());
    assert (traits.enPassantPawns() <= squares.any(Rank5));
    traits.clearEnPassants();
}

bool PositionSide::isPinned(Bb occupied) const {
    for (Pi pinner : pinners()) {
        Bb pinLine = ::inBetween(opKing, sq(pinner));
        assert (pinLine.isAny());
        if (pinLine.isNone(occupied)) {
            return true;
        }
    }

    return false;
}

bool PositionSide::dropValid(Piece piece, Square to) {
    if (bbSide_.has(to)) {
        io::error("invalid fen: square already occupied");
        return false;
    }
    bbSide_ += Bb{to};

    Pi pi{ piece.is(King) ? PiKing : PieceSet{any() | PiMask{PiKing}}.piFirstVacant() };

    material_.drop(piece);
    types.drop(pi, piece);
    squares.drop(pi, to);

    if (piece.is(Pawn)) {
        if (to.isOn(Rank1) || to.isOn(Rank8)) {
            io::error("invalid fen: pawn on impossible rank");
            return false;
        }
        if (to.isOn(Rank7)) { traits.setPromotable(pi);}
        bbPawns_ += Bb{to};
        bbPawnAttacks_ = bbPawns_.forwardDiag();
    }

    assertOk(pi, piece, to);
    return true;
}

bool PositionSide::setValidCastling(CastlingSide castlingSide) {
    if (!sqKing().isOn(Rank1)) {
        io::error("invalid fen castling: king on bad rank");
        return false;
    }

    Square sqOuter{ sqKing() };
    for (Pi piRook : types.any(Rook) & any(Rank1)) {
        if (CastlingRules::castlingSide(sqOuter, sq(piRook)).is(*castlingSide)) {
            sqOuter = sq(piRook);
        }
    }
    if (sqOuter == sqKing()) {
        io::error("invalid fen castling: no castling rook found");
        return false;
    }

    Pi piRook{ pi(sqOuter) };
    if (isCastling(piRook)) {
        io::error("invalid fen castling: rook is already set castling");
        return false;
    }

    traits.setCastling(piRook);
    return true;
}

bool PositionSide::setValidCastling(File file) {
    if (!sqKing().isOn(Rank1)) {
        io::error("invalid fen castling: king on bad rank");
        return false;
    }

    Square rookFrom(file, Rank1);
    if (!has(rookFrom)) {
        io::error("invalid fen castling: no castling piece found");
        return false;
    }

    Pi piRook{ pi(rookFrom) };
    if (!types.isRook(piRook)) {
        io::error("invalid fen castling: castling piece is not rook");
        return false;
    }
    if (isCastling(piRook)) {
        io::error("invalid fen castling: rook is already set castling");
        return false;
    }

    traits.setCastling(piRook);
    return true;
}
