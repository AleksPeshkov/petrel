#include "Position.hpp"

void Position::clear() {
    accumulator.clear();
}

Score Position::evaluate() const {
    auto eval = accumulator.evaluate();
    return Score::clampEval(eval);
}

void Position::copyParent(const Position* parent) {
    // copy from the parent position but swap sides
    assert (parent);
    accumulator.copyParent(parent->accumulator);
    positionSide_[Side{My}] = parent->OP;
    positionSide_[Side{Op}] = parent->MY;
    rule50_ = parent->rule50_;
}

void Position::makeMove(Square from, Square to) {
    PositionSide::swap(MY, OP);
    accumulator.swap();

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op, ZobristAndEval>(from, to);
    zobrist_.flip();
    //assert (zobrist() == generateZobrist()); // true, but slow to compute
}

void Position::makeMoveNoEval(Square from, Square to) {
    PositionSide::swap(MY, OP);
    //skip accumulator.swap();

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op, WithZobrist>(from, to);
    zobrist_.flip();
    //assert (z() == generateZobrist().v()); // true, but slow to compute
}

void Position::makeNullMove(const Position* parent) {
    copyParent(parent);
    zobrist_ = parent->zobrist_;
    // null move
    rule50_.next();
    occupied_[Side{My}] = parent->occupied_[Side{Op}];
    occupied_[Side{Op}] = parent->occupied_[Side{My}];

    // clear en passant status from the previous move
    if (MY.hasEnPassant()) {
        zobrist_.opEnPassant(MY.sqEnPassant());
        OP.clearEnPassantKillers();
        MY.clearEnPassantVictim();
    }

    zobrist_.flip();
    //assert (z() == generateZobrist().v()); // true, but slow to compute
}

void Position::makeMove(const Position* parent, Square from, Square to) {
    copyParent(parent);
    zobrist_ = parent->zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, ZobristAndEval>(from, to);
    zobrist_.flip();

    //assert (z() == parent->createZobrist(from, to).v()); // true, but slow to compute
    //assert (z() == generateZobrist().v()); // true, but slow to compute
}

void Position::makeMoveNoZobrist(const Position* parent, Square from, Square to) {
    copyParent(parent);

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, WithEval>(from, to);

    //assert (z() == Z{} || z() == generateZobrist().v()); // true, but slow to compute
}

template <Side::_t My, Position::MakeMoveFlags Flags>
void Position::makeMove(Square from, Square to) {
    constexpr Side::_t Op{~My};

    rule50_.next(); // will be reset later if the move is a capture or pawn move

    // assumes that the given move is valid and legal
    assert (MY.checkers().none());
    OP.clearCheckers();

    Pi pi = MY.pi(from);

    // clear en passant status from the previous move
    if (OP.hasEnPassant()) {
        if constexpr (Flags & WithZobrist) {
            zobrist_.opEnPassant(OP.sqEnPassant());
        }
        MY.clearEnPassantKillers();

        // en passant capture encoded as the pawn captures the pawn
        if (MY.isPawn(pi) && from.on(Rank5) && to.on(Rank5)) {
            rule50_.clear();

            Square ep{to};
            to = Square{File{to}, Rank6};

            if constexpr (Flags & WithZobrist) {
                zobrist_.move(Pawn, from, to);
                zobrist_.opCapture(NonKingType{Pawn}, ~ep);
            }
            MY.movePawn(pi, from, to);
            OP.capture(~ep); // also clears en passant victim

            if constexpr (Flags & WithEval) {
                accumulator.ep(from, to, ep);
            }
            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));
            return; // end of en passant capture move
        }

        OP.clearEnPassantVictim();
    }

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    if (MY.isPawn(pi)) {
        rule50_.clear();

        if (from.on(Rank7)) {
            PromoType promo{::promoTypeFrom(Rank{to})};
            to = {File{to}, Rank8};

            if constexpr (Flags & WithZobrist) {
                zobrist_.promote(from, promo, to);
            }
            pi = MY.piPromoted(pi, from, promo, to);

            if (OP.has(~to)) {
                NonKingType captured{OP.typeAt(~to).v()};
                if constexpr (Flags & WithZobrist) {
                    if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                }
                OP.capture(~to);

                if constexpr (Flags & WithEval) {
                    accumulator.promote(promo, from, to, captured);
                }
                updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
                return; // end of pawn promotion move with capture
            }

            if constexpr (Flags & WithEval) {
                accumulator.promote(promo, from, to);
            }
            updateSliderAttacks<My>(MY.affectedBy(from, to) | pi, OP.affectedBy(~from, ~to));
            return; // end of pawn promotion move without capture
        }

        if constexpr (Flags & WithZobrist) {
            zobrist_.move(Pawn, from, to);
        }
        MY.movePawn(pi, from, to);

        // possible en passant capture and capture with promotion already treated
        if (OP.has(~to)) {
            NonKingType captured{OP.typeAt(~to).v()};
            if constexpr (Flags & WithZobrist) {
                zobrist_.opCapture(captured, ~to);
            }
            OP.capture(~to);

            if constexpr (Flags & WithEval) {
                accumulator.move(Pawn, from, to, captured);
            }
            updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));
            return; // end of simple pawn capture move
        }

        if constexpr (Flags & WithEval) {
            accumulator.move(Pawn, from, to);
        }
        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if (from.on(Rank2) && to.on(Rank4)) {
            setLegalEnPassant<My>(pi, to);
            if constexpr (Flags & WithZobrist) {
                if (MY.hasEnPassant()) { zobrist_.enPassant(MY.sqEnPassant()); }
            }
            return; // end of pawn double push move
        }

        return; // end of simple pawn push move
    }

    if (MY.sqKing().is(from)) {
        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
            zobrist_.move(King, from, to);
        }
        MY.moveKing(from, to);
        OP.setOpKing(~to);

        if (OP.has(~to)) {
            rule50_.clear();
            NonKingType captured{OP.typeAt(~to).v()};
            if constexpr (Flags & WithZobrist) {
                if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.opCapture(captured, ~to);
            }
            OP.capture(~to);

            if constexpr (Flags & WithEval) {
                accumulator.move(King, from, to, captured);
            }
            updateSliderAttacks<My>(MY.affectedBy(from));
            return; // end of king capture move
        }

        if constexpr (Flags & WithEval) {
            accumulator.move(King, from, to);
        }
        updateSliderAttacks<My>(MY.affectedBy(from, to));
        return; // end of king non-capture move
    }

    // castling move encoded as castling rook captures own king
    if (MY.sqKing().is(to)) {
        Square rookFrom = from;
        Square kingFrom = to;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
            zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
        }
        MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);
        OP.setOpKing(~kingTo);

        if constexpr (Flags & WithEval) {
            accumulator.castle(kingFrom, kingTo, rookFrom, rookTo);
        }
        //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
        //TRICK: castling rook should attack 'kingFrom' square
        //TRICK: only first rank sliders can be affected
        updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.piecesOn(Rank1));
        return; //end of castling move
    }

    // simple non-pawn non-king move:
    PieceType moved = MY.typeOf(pi);

    if constexpr (Flags & WithZobrist) {
        if (MY.isCastling(pi)) { zobrist_.castling(from); } // move of the rook with castling right
        zobrist_.move(moved, from, to);
    }
    MY.move(pi, from, to);

    if (OP.has(~to)) {
        rule50_.clear();
        NonKingType captured{OP.typeAt(~to).v()};
        if constexpr (Flags & WithZobrist) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
        }
        OP.capture(~to);

        if constexpr (Flags & WithEval) {
            accumulator.move(moved, from, to, captured);
        }
        updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
        return; // end of simple non-pawn non-king capture move
    }

    if constexpr (Flags & WithEval) {
        accumulator.move(moved, from, to);
    }
    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
    return; // end of simple non-pawn non-king quiet move
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected) {
    constexpr Side::_t Op{~My};

    occupied_[Side{My}] = MY.bbSide() + ~OP.bbSide();
    occupied_[Side{Op}] = OP.bbSide() + ~MY.bbSide();

    myAffected &= MY.sliders();
    if (myAffected.any()) {
        MY.updateSlidersCheckers(myAffected, OCCUPIED);
    }
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected, PiMask opAffected) {
    constexpr Side::_t Op{~My};

    updateSliderAttacks<My>(myAffected);

    opAffected &= OP.sliders();
    if (opAffected.any()) {
        OP.updateSliders(opAffected, OP_OCCUPIED);
    }
}

template <Side::_t My>
void Position::setLegalEnPassant(Pi victim, Square to) {
    constexpr Side::_t Op{~My};

    assert (MY.isPawn(victim));
    assert (MY.sq(victim).is(to));
    assert (to.on(Rank4));

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    Square ep{File{to}, Rank3};

    // check if there are any pawns to capture ep victim
    Bb killers = ~OP.bbPawns() & ::attacksFrom(Pawn, ep);
    if (killers.none()) { return; }

    // discovered check
    if (MY.isPinned(OCCUPIED)) { assert ((MY.checkers() % victim).any()); return; }
    assert ((MY.checkers() % victim).none());

    for (Square from : killers) {
        assert (from.on(Rank4));

        if (!MY.isPinned(OCCUPIED - Bb{from} + Bb{ep} - Bb{to})) {
            MY.setEnPassantVictim(victim);
            OP.setEnPassantKiller(OP.pi(~from));
        }
    }
}

bool Position::dropValid(Side si, PieceType ty, Square to) {
    accumulator.drop(si, ty, to);
    return positionSide(si).dropValid(ty, to);
}

bool Position::afterDrop() {
    PositionSide::finalSetup(MY, OP);
    updateSliderAttacks<Op>(OP.pieces(), MY.pieces());
    rule50_.clear();

    //opponent should not be in check
    return MY.checkers().none();
}

Bb Position::bbPassedPawns() const {
    Bb blockers = ~(OP.bbPawns() | OP.bbPawnAttacks() >> 8u);
    for (int i = 0; i < 5; ++i) {
        blockers |= blockers << 8u;
    }
    return MY.bbPawns() % blockers;
}

bool Position::isSpecialMove(Square from, Square to) const {
    if (MY.sqKing().is(to)) {
        return true; // castling
    }

    if (MY.isPawn(MY.pi(from))) {
        if (from.on(Rank7)) {
            return true; // pawn promotion
        }
        if (from.on(Rank5) && to.on(Rank5)) {
            return true; // en passant
        }
    }

    return false; // normal move
}

template <Side::_t My>
Zobrist Position::generateZobrist() const {
    Zobrist z{};

    for (Pi pi : MY.pieces()) { z(MY.typeOf(pi), MY.sq(pi));}
    for (Pi rook : MY.castlingRooks()) { z.castling(MY.sq(rook)); }

    return z;
}

Zobrist Position::generateZobrist() const {
    constexpr Side::_t Op{~My};

    Zobrist z{generateZobrist<My>(), generateZobrist<Op>()};
    if (OP.hasEnPassant()) { z.opEnPassant(OP.sqEnPassant()); }

    return z;
}

Zobrist Position::createZobrist(Square from, Square to) const {
    Zobrist mz{zobrist_}; // side to move pieces hash
    Zobrist oz{}; // opponent side pieces hash

    Pi pi{MY.pi(from)};
    PieceType ty{MY.typeOf(pi)};

    if (OP.hasEnPassant()) {
        // clear en passant tag
        oz.enPassant(OP.sqEnPassant());

        // actual en passant capture
        if (ty.is(Pawn) && from.on(Rank5) && to.on(Rank5)) {
            mz.move(Pawn, from, Square{File{to}, Rank6});
            oz(Pawn, ~to);
            return Zobrist{oz, mz};
        }
    }

    do {
        if (ty.is(Pawn)) {
            if (from.on(Rank7)) {
                PromoType promo{::promoTypeFrom(Rank{to})};
                to = Square{File{to}, Rank8};

                mz.promote(from, promo, to);
                break; // goto handle captured piece if any
            }

            if (from.on(Rank2) && to.on(Rank4)) {
                mz.move(PieceType{Pawn}, from, to);

                File file{from};
                Square ep{file, Rank3};

                Bb killers = ~OP.bbPawns() & ::attacksFrom(Pawn, ep);
                if (killers.any() && !MY.isPinned(OCCUPIED - Bb{from} + Bb{ep})) {
                    for (Square killer : killers) {
                        assert (killer.on(Rank4));

                        if (!MY.isPinned(OCCUPIED - Bb{killer} + Bb{ep})) {
                            // strictly legal en passant tag
                            //TODO: set en passant traits here and skip legality check again
                            mz.enPassant(to);
                            return Zobrist{oz, mz};
                        }
                    }
                }
                return Zobrist{oz, mz};
            }

            // common pawn move (with capture and non-capture)
            mz.move(PieceType{Pawn}, from, to);
            break; // goto handle captured piece if any
        }

        if (MY.sqKing().is(to)) {
            //castling move encoded as rook moves over own king's square
            Square kingFrom = to;
            Square rookFrom = from;
            Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
            Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

            // clear all castling rights
            for (Pi rook : MY.castlingRooks()) { mz.castling(MY.sq(rook)); }

            mz.castle(kingFrom, kingTo, rookFrom, rookTo);
            return Zobrist{oz, mz};
        }
        else if (ty.is(King)) {
            // clear all castling rights
            for (Pi rook : MY.castlingRooks()) { mz.castling(MY.sq(rook)); }
        }
        else if (MY.isCastling(pi)) {
            // clear the moved rook castling right
            assert (ty.is(Rook));
            mz.castling(from);
        }

        // common non-pawn move
        mz.move(ty, from, to);
    } while (false);

// handle captured piece if any
    if (OP.has(~to)) {
        Pi victim = OP.pi(~to);
        oz(OP.typeOf(victim), ~to);

        if (OP.isCastling(victim)) { oz.castling(~to); }
    }

    // side to move changes after a move, so we flip zobrist here
    return Zobrist{oz, mz};
}
