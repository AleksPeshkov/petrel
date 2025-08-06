#include "Position.hpp"
#include "CastlingRules.hpp"
#include "AttacksFrom.hpp"


void Position::makeMove(Square from, Square to) {
    PositionSide::swap(MY, OP);

    //the position just swapped its sides, so we make the move for the Op
    makeMove<Op>(from, to);
    zobrist.flip();
    //assert (zobrist == generateZobrist()); // true, but slow to compute
}

void Position::makeMove(const Position* parent, Square from, Square to) {
    assert (parent);
    MY = (*parent)[Op];
    OP = (*parent)[My];
    //current position flipped its sides relative to parent, so we make the move inplace for the Op

    zobrist = parent->zobrist;
    makeMove<Op>(from, to);
    zobrist.flip();

    //assert (zobrist == generateZobrist()); // true, but slow to compute
}

void Position::makeMoveNoZobrist(const Position* parent, Square from, Square to) {
    assert (parent);
    MY = (*parent)[Op];
    OP = (*parent)[My];
    //current position flipped its sides relative to parent, so we make the move inplace for the Op

    makeMove<Op, NoZobrist>(from, to);

    //assert (zobrist == Zobrist{} || zobrist == generateZobrist()); // true, but slow to compute
}

template <Side::_t My, Position::UpdateZobrist Z>
void Position::makeMove(Square from, Square to) {
    constexpr Side Op{~My};

    lastPieceTo = to; // will be corrected later in case of castling, underpromotion and en passant capture

    //Assumes that the given move is valid and legal
    assert (MY.checkers().none());
    OP.clearCheckers();

    Pi pi = MY.pieceAt(from);

    //clear en passant status from the previous move
    if (OP.hasEnPassant()) {
        if constexpr (Z) {
            zobrist.opEnPassant(OP.enPassantSquare());
        }
        MY.clearEnPassantKillers();

        //en passant capture encoded as the pawn captures the pawn
        if (MY.isPawn(pi) && from.on(Rank5) && to.on(Rank5)) {
            Square ep{File(to), Rank6};
            lastPieceTo = ep;

            if constexpr (Z) {
                zobrist.move(Pawn, from, ep);
                zobrist.op(Pawn, ~to);
            }
            MY.movePawn(pi, from, ep);
            OP.capture(~to); //also clears en passant victim

            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));
            return; //end of en passant capture move
        }

        OP.clearEnPassantVictim();
    }

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    if (MY.isPawn(pi)) {
        if (from.on(Rank7)) {
            PromoType promo = ::promoTypeFrom(Rank{to});
            to = {File(to), Rank8};
            lastPieceTo = to;

            if constexpr (Z) {
                zobrist.promote(from, promo, to);
            }
            MY.promote(pi, from, promo, to);

            if (OP.has(~to)) {
                if constexpr (Z) {
                    if (OP.isCastling(~to)) { zobrist.opCastling(~to); } // captured the rook with castling right
                    zobrist.op(OP.typeOf(~to), ~to);
                }
                OP.capture(~to);

                updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
                return; //end of pawn promotion move with capture
            }

            updateSliderAttacks<My>(MY.affectedBy(from, to) | pi, OP.affectedBy(~from, ~to));
            return; //end of pawn promotion move without capture
        }

        if constexpr (Z) {
            zobrist.move(Pawn, from, to);
        }
        MY.movePawn(pi, from, to);

        //possible en passant capture and capture with promotion already treated
        if (OP.has(~to)) {
            if constexpr (Z) {
                zobrist.op(OP.typeOf(~to), ~to);
            }
            OP.capture(~to);

            updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));
            return; //end of simple pawn capture move
        }

        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if (from.on(Rank2) && to.on(Rank4)) {
            setLegalEnPassant<My>(pi, to);
            if constexpr (Z) {
                if (MY.hasEnPassant()) { zobrist.myEnPassant(MY.enPassantSquare()); }
            }
            return; //end of pawn double push move
        }

        return; //end of simple pawn push move
    }

    if (MY.kingSquare().is(from)) {
        if constexpr (Z) {
            for (Pi rook : MY.castlingRooks()) { zobrist.myCastling(MY.squareOf(rook)); }
            zobrist.move(King, from, to);
        }
        MY.moveKing(from, to);
        OP.setOpKing(~to);

        if (OP.has(~to)) {
            if constexpr (Z) {
                if (OP.isCastling(~to)) { zobrist.opCastling(~to); } // captured the rook with castling right
                zobrist.op(OP.typeOf(~to), ~to);
            }
            OP.capture(~to);

            updateSliderAttacks<My>(MY.affectedBy(from));
            return; // end of king capture move
        }

        updateSliderAttacks<My>(MY.affectedBy(from, to));
        return; // end of king non-capture move
    }

    //castling move encoded as castling rook captures own king
    if (MY.kingSquare().is(to)) {
        Square rookFrom = from;
        Square kingFrom = to;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);
        lastPieceTo = rookTo;

        if constexpr (Z) {
            for (Pi rook : MY.castlingRooks()) { zobrist.myCastling(MY.squareOf(rook)); }
            zobrist.my(King, kingFrom); zobrist.my(King, kingTo);
            zobrist.my(Rook, from); zobrist.my(Rook, rookTo);
        }
        MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);
        OP.setOpKing(~kingTo);

        //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
        //TRICK: castling rook should attack 'kingFrom' square
        //TRICK: only first rank sliders can be affected
        updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.piecesOn(Rank1));
        return; //end of castling move
    }

    //simple non-pawn non-king move

    if constexpr (Z) {
        if (MY.isCastling(pi)) { zobrist.myCastling(from); } // move of the rook with castling right
        zobrist.move(MY.typeOf(pi), from, to);
    }
    MY.move(pi, from, to);

    if (OP.has(~to)) {
        if constexpr (Z) {
            if (OP.isCastling(~to)) { zobrist.opCastling(~to); } // captured the rook with castling right
            zobrist.op(OP.typeOf(~to), ~to);
        }
        OP.capture(~to);

        updateSliderAttacks<My>(MY.affectedBy(from) | pi, OP.affectedBy(~from));
        return; //end of simple non-pawn non-king capture move
    }

    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
    return; //end of simple non-pawn non-king move
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected) {
    constexpr Side Op{~My};

    occupiedBb[My] = MY.piecesBb + ~OP.piecesBb;
    occupiedBb[Op] = OP.piecesBb + ~MY.piecesBb;

    myAffected &= MY.sliders();
    if (myAffected.any()) {
        MY.updateSlidersCheckers(myAffected, occupiedBb[My]);
    }
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected, PiMask opAffected) {
    constexpr Side Op{~My};

    updateSliderAttacks<My>(myAffected);

    opAffected &= OP.sliders();
    if (opAffected.any()) {
        OP.updateSliders(opAffected, occupiedBb[Op]);
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
    if (MY.isPinned(occupiedBb[My])) { assert ((MY.checkers() % victim).any()); return; }
    assert ((MY.checkers() % victim).none());

    for (Square from : killers) {
        assert (from.on(Rank4));

        if (!MY.isPinned(occupiedBb[My] - from + ep - to)) {
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
    lastPieceTo = OP.kingSquare(); // set something sensible

    //opponent should not be in check
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

Zobrist Position::generateZobrist() const {
    auto mz = MY.generateZobrist();
    auto oz = OP.generateZobrist();

    if (OP.hasEnPassant()) {
        oz.setEnPassant(OP.enPassantSquare());
    }

    return Zobrist(mz, oz);
}

Zobrist Position::createZobrist(Square from, Square to) const {
    // side to move pieces encoding
    Zobrist mz{zobrist};

    // opponet side pieces encoding
    Zobrist oz{0};

    Pi pi = MY.pieceAt(from);
    PieceType ty = MY.typeOf(pi);

    if (OP.hasEnPassant()) {
        oz.clearEnPassant(OP.enPassantSquare());

        //en passant capture
        if (ty.is(Pawn) && from.on(Rank5) && to.on(Rank5)) {
            mz.move(Pawn, from, {File{to}, Rank6});
            oz.clear(Pawn, ~to);
            goto zobrist;
        }
    }

    if (ty.is(Pawn)) {
        if (from.on(Rank7)) {
            PieceType promo = ::pieceTypeFrom(Rank{to});
            to = {File{to}, Rank8};

            mz.clear(Pawn, from);
            mz.drop(promo, to);
            goto capture;
        }

        if (from.on(Rank2) && to.on(Rank4)) {
            mz.move(ty, from, to);

            File file = File{from};
            Square ep{file, Rank3};

            Bb killers = ~OP.pawnsSquares() & ::attacksFrom(Pawn, ep);
            if (killers.any() && !MY.isPinned(occupiedBb[My] - from + ep)) {
                for (Square killer : killers) {
                    assert (killer.on(Rank4));

                    if (!MY.isPinned(occupiedBb[My] - killer + ep)) {
                        mz.setEnPassant(to);
                        goto zobrist;
                    }
                }
            }
            goto zobrist;
        }

        //the rest of pawns moves (non-promotion, non en passant, non double push)
    }
    else if (MY.kingSquare().is(to)) {
        //castling move encoded as rook moves over own king's square
        for (Pi rook : MY.castlingRooks()) {
            mz.clearCastling(MY.squareOf(rook));
        }

        Square kingFrom = to;
        Square rookFrom = from;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

        mz.clear(King, kingFrom);
        mz.clear(Rook, rookFrom);
        mz.drop(King, kingTo);
        mz.drop(Rook, rookTo);
        goto zobrist;
    }
    else if (ty.is(King)) {
        for (Pi rook : MY.castlingRooks()) {
            mz.clearCastling(MY.squareOf(rook));
        }
    }
    else if (MY.isCastling(pi)) {
        //move of the rook with castling rights
        assert (ty.is(Rook));
        mz.clearCastling(from);
    }

    mz.move(ty, from, to);

capture:
    if (OP.has(~to)) {
        Pi victim = OP.pieceAt(~to);
        oz.clear(OP.typeOf(victim), ~to);

        if (OP.isCastling(victim)) {
            oz.clearCastling(~to);
        }
    }

zobrist:
    return Zobrist{oz, mz};
}
