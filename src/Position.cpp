#include "Position.hpp"

void Position::makeMove(Square from, Square to) {
    PositionSide::swap(MY, OP);

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op>(from, to);
    zobrist_.flip();
    //assert (zobrist() == generateZobrist()); // true, but slow to compute
}

void Position::makeNullMove(const Position* parent) {
    // copy from the parent position but swap sides
    assert (parent);
    positionSide_[Side{My}] = parent->OP;
    positionSide_[Side{Op}] = parent->MY;
    zobrist_ = parent->zobrist_;
    rule50_ = parent->rule50_;

    // null move
    rule50_.next();
    occupied_[Side{My}] = parent->occupied_[Side{Op}];
    occupied_[Side{Op}] = parent->occupied_[Side{My}];

    // clear en passant status from the previous move
    if (MY.hasEnPassant()) {
        zobrist_.opEnPassant(MY.enPassantSquare());
        OP.clearEnPassantKillers();
        MY.clearEnPassantVictim();
    }

    zobrist_.flip();
    //assert (zobrist() == generateZobrist()); // true, but slow to compute
}

void Position::makeMove(const Position* parent, Square from, Square to) {
    // copy from the parent position but swap sides
    assert (parent);
    positionSide_[Side{My}] = parent->OP;
    positionSide_[Side{Op}] = parent->MY;
    zobrist_ = parent->zobrist_;
    rule50_ = parent->rule50_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op>(from, to);
    zobrist_.flip();

    //assert (zobrist() == parent->createZobrist(from, to)); // true, but slow to compute
    //assert (zobrist() == generateZobrist()); // true, but slow to compute
}

void Position::makeMoveNoZobrist(const Position* parent, Square from, Square to) {
    // copy from the parent position but swap sides
    assert (parent);
    positionSide_[Side{My}] = parent->OP;
    positionSide_[Side{Op}] = parent->MY;
    rule50_ = parent->rule50_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, NoZobrist>(from, to);

    //assert (zobrist() == Zobrist{} || zobrist() == generateZobrist()); // true, but slow to compute
}

template <Side::_t My, Position::UpdateZobrist Z>
void Position::makeMove(Square from, Square to) {
    constexpr Side::_t Op{~My};

    rule50_.next(); // will be reset later if the move is a capture or pawn move

    // assumes that the given move is valid and legal
    assert (MY.checkers().none());
    OP.clearCheckers();

    Pi pi = MY.piAt(from);

    // clear en passant status from the previous move
    if (OP.hasEnPassant()) {
        if constexpr (Z) {
            zobrist_.opEnPassant(OP.enPassantSquare());
        }
        MY.clearEnPassantKillers();

        // en passant capture encoded as the pawn captures the pawn
        if (MY.isPawn(pi) && from.on(Rank5) && to.on(Rank5)) {
            rule50_.clear();

            Square ep{to};
            to = Square{File{to}, Rank6};

            if constexpr (Z) {
                zobrist_.move(PieceType{Pawn}, from, to);
                zobrist_.op(NonKingType{Pawn}, ~ep);
            }
            MY.movePawn(pi, from, to);
            OP.capture(~ep); // also clears en passant victim

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

            if constexpr (Z) {
                zobrist_.promote(from, promo, to);
            }
            pi = MY.piPromoted(pi, from, promo, to);

            if (OP.has(~to)) {
                if constexpr (Z) {
                    if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.op(NonKingType{OP.typeAt(~to)}, ~to);
                }
                OP.capture(~to);

                updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
                return; // end of pawn promotion move with capture
            }

            updateSliderAttacks<My>(MY.affectedBy(from, to) | pi, OP.affectedBy(~from, ~to));
            return; // end of pawn promotion move without capture
        }

        if constexpr (Z) {
            zobrist_.move(PieceType{Pawn}, from, to);
        }
        MY.movePawn(pi, from, to);

        // possible en passant capture and capture with promotion already treated
        if (OP.has(~to)) {
            if constexpr (Z) {
                zobrist_.op(NonKingType{OP.typeAt(~to)}, ~to);
            }
            OP.capture(~to);

            updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));
            return; // end of simple pawn capture move
        }

        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if (from.on(Rank2) && to.on(Rank4)) {
            setLegalEnPassant<My>(pi, to);
            if constexpr (Z) {
                if (MY.hasEnPassant()) { zobrist_.enPassant(MY.enPassantSquare()); }
            }
            return; // end of pawn double push move
        }

        return; // end of simple pawn push move
    }

    if (MY.kingSquare().is(from)) {
        if constexpr (Z) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.squareOf(rook)); }
            zobrist_.move(PieceType{King}, from, to);
        }
        MY.moveKing(from, to);
        OP.setOpKing(~to);

        if (OP.has(~to)) {
            rule50_.clear();
            if constexpr (Z) {
                if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.op(NonKingType{OP.typeAt(~to)}, ~to);
            }
            OP.capture(~to);

            updateSliderAttacks<My>(MY.affectedBy(from));
            return; // end of king capture move
        }

        updateSliderAttacks<My>(MY.affectedBy(from, to));
        return; // end of king non-capture move
    }

    // castling move encoded as castling rook captures own king
    if (MY.kingSquare().is(to)) {
        Square rookFrom = from;
        Square kingFrom = to;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

        if constexpr (Z) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.squareOf(rook)); }
            zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
        }
        MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);
        OP.setOpKing(~kingTo);

        //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
        //TRICK: castling rook should attack 'kingFrom' square
        //TRICK: only first rank sliders can be affected
        updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.piecesOn(Rank1));
        return; //end of castling move
    }

    // simple non-pawn non-king move:

    if constexpr (Z) {
        if (MY.isCastling(pi)) { zobrist_.castling(from); } // move of the rook with castling right
        zobrist_.move(MY.typeOf(pi), from, to);
    }
    MY.move(pi, from, to);

    if (OP.has(~to)) {
        rule50_.clear();
        if constexpr (Z) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.op(NonKingType{OP.typeAt(~to)}, ~to);
        }
        OP.capture(~to);

        updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
        return; // end of simple non-pawn non-king capture move
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
        OP.updateSliders(opAffected, occupied(Side{Op}));
    }
}

template <Side::_t My>
void Position::setLegalEnPassant(Pi victim, Square to) {
    constexpr Side::_t Op{~My};

    assert (MY.isPawn(victim));
    assert (MY.squareOf(victim).is(to));
    assert (to.on(Rank4));

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    Square ep{File{to}, Rank3};

    // check if there are any pawns to capture ep victim
    Bb killers = ~OP.bbPawns() & ::attacksFrom(PieceType{Pawn}, ep);
    if (killers.none()) { return; }

    // discovered check
    if (MY.isPinned(OCCUPIED)) { assert ((MY.checkers() % victim).any()); return; }
    assert ((MY.checkers() % victim).none());

    for (Square from : killers) {
        assert (from.on(Rank4));

        if (!MY.isPinned(OCCUPIED - Bb{from} + Bb{ep} - Bb{to})) {
            MY.setEnPassantVictim(victim);
            OP.setEnPassantKiller(OP.piAt(~from));
        }
    }
}

bool Position::dropValid(Side My, PieceType ty, Square to) {
    return MY.dropValid(ty, to);
}

bool Position::afterDrop() {
    PositionSide::finalSetup(MY, OP);
    updateSliderAttacks<Op>(OP.pieces(), MY.pieces());
    rule50_.clear();

    //opponent should not be in check
    return MY.checkers().none();
}

bool Position::isSpecialMove(Square from, Square to) const {
    if (MY.kingSquare().is(to)) {
        return true; // castling
    }

    if (MY.isPawn(MY.piAt(from))) {
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
    Zobrist z{0};

    for (Pi pi : MY.pieces()) { z(MY.typeOf(pi), MY.squareOf(pi));}
    for (Pi rook : MY.castlingRooks()) { z.castling(MY.squareOf(rook)); }

    return z;
}

Zobrist Position::generateZobrist() const {
    constexpr Side::_t Op{~My};

    Zobrist z{generateZobrist<My>(), generateZobrist<Op>()};
    if (OP.hasEnPassant()) { z.opEnPassant(OP.enPassantSquare()); }

    return z;
}

Zobrist Position::createZobrist(Square from, Square to) const {
    // side to move pieces hash
    Zobrist mz{zobrist_};

    // opponent side pieces hash
    Zobrist oz{0};

    Pi pi = MY.piAt(from);
    PieceType ty = MY.typeOf(pi);

    if (OP.hasEnPassant()) {
        oz.enPassant(OP.enPassantSquare());

        // en passant capture
        if (ty.is(Pawn) && from.on(Rank5) && to.on(Rank5)) {
            mz.move(PieceType{Pawn}, from, Square{File{to}, Rank6});
            oz(PieceType{Pawn}, ~to);
            goto zobrist;
        }
    }

    if (ty.is(Pawn)) {
        if (from.on(Rank7)) {
            PromoType promo{::promoTypeFrom(Rank{to})};
            to = Square{File{to}, Rank8};

            mz.promote(from, promo, to);
            goto capture;
        }

        if (from.on(Rank2) && to.on(Rank4)) {
            mz.move(ty, from, to);

            File file{from};
            Square ep{file, Rank3};

            Bb killers = ~OP.bbPawns() & ::attacksFrom(PieceType{Pawn}, ep);
            if (killers.any() && !MY.isPinned(OCCUPIED - Bb{from} + Bb{ep})) {
                for (Square killer : killers) {
                    assert (killer.on(Rank4));

                    if (!MY.isPinned(OCCUPIED - Bb{killer} + Bb{ep})) {
                        mz.enPassant(to);
                        goto zobrist;
                    }
                }
            }
            goto zobrist;
        }

        // fall though the rest of pawns moves (non-promotion, non en passant, non double push)
    }
    else if (MY.kingSquare().is(to)) {
        Square kingFrom = to;
        Square rookFrom = from;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

        //castling move encoded as rook moves over own king's square
        for (Pi rook : MY.castlingRooks()) {
            mz.castling(MY.squareOf(rook));
        }

        mz.castle(kingFrom, kingTo, rookFrom, rookTo);
        goto zobrist;
    }
    else if (ty.is(King)) {
        for (Pi rook : MY.castlingRooks()) { mz.castling(MY.squareOf(rook)); }
    }
    else if (MY.isCastling(pi)) {
        //move of the rook with castling rights
        assert (ty.is(Rook));
        mz.castling(from);
    }

    mz.move(ty, from, to);

capture:
    if (OP.has(~to)) {
        Pi victim = OP.piAt(~to);
        oz(OP.typeOf(victim), ~to);

        if (OP.isCastling(victim)) { oz.castling(~to); }
    }

zobrist:
    return Zobrist{oz, mz};
}
