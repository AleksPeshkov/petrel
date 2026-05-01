
#include "Position.hpp"

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
void Position::setLegalEnPassant(Square ep) {
    constexpr Side::_t Op{~My};

    assert (ep.on(Rank4));
    assert (MY.isPawn(ep));
    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    Square to{ep.file(), Rank3}; // attacking pawn destination square

    // check if there are any pawns to capture ep victim
    Bb killers = ~OP.bbPawns() & ::attacksFrom(Pawn, to);
    if (killers.none()) { return; }

    // discovered check
    if (MY.isPinned(OCCUPIED)) { assert ((MY.checkers() % PiMask{MY.pi(ep)}).any()); return; }
    assert ((MY.checkers() % PiMask{MY.pi(ep)}).none());

    for (Square from : killers) {
        assert (from.on(Rank4));

        if (!MY.isPinned(OCCUPIED - Bb{from} + Bb{to} - Bb{ep})) {
            MY.setEnPassantVictim(ep);
            OP.setEnPassantKiller(~from);
        }
    }
}

template <Side::_t My, Position::MakeMoveFlags Flags>
void Position::makeMove(Square from, Square to, auto&& prefetch) {
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
        if (MY.isPawn(from) && from.on(Rank5) && to.on(Rank5)) {
            Square ep{to};
            to = Square{to.file(), Rank6};

            if constexpr (Flags & WithZobrist) {
                zobrist_.move(Pawn, from, to);
                zobrist_.opCapture(NonKingType{Pawn}, ~ep);
                prefetch();
            }

            rule50_.clear();
            MY.movePawn(pi, from, to);
            OP.capture(~ep); // also clears en passant victim
            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));

            if constexpr (Flags & WithEval) { accumulator.ep(from, to, ep); }
            return; // end of en passant capture move
        }

        OP.clearEnPassantVictim();
    }

    assert (!MY.hasEnPassant());
    assert (!OP.hasEnPassant());

    if (MY.isPawn(from)) {
        rule50_.clear();

        if (from.on(Rank7)) {
            PromoType promo{::promoTypeFrom(to.rank())};
            to = {to.file(), Rank8};

            if constexpr (Flags & WithZobrist) {
                zobrist_.promote(from, promo, to);
            }
            pi = MY.piPromoted(pi, from, promo, to);

            if (OP.has(~to)) {
                NonKingType captured{*OP.typeAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                    prefetch();
                }
                OP.capture(~to);
                updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{pi}, OP.affectedBy(~from));

                if constexpr (Flags & WithEval) { accumulator.promote(from, promo, to, captured); }
                return; // end of pawn promotion move with capture
            }

            if constexpr (Flags & WithZobrist) { prefetch(); }
            updateSliderAttacks<My>(MY.affectedBy(from, to) | PiMask{pi}, OP.affectedBy(~from, ~to));

            if constexpr (Flags & WithEval) { accumulator.promote(from, promo, to); }
            return; // end of pawn promotion move without capture
        }

        if constexpr (Flags & WithZobrist) {
            zobrist_.move(Pawn, from, to);
        }
        MY.movePawn(pi, from, to);

        // possible en passant capture and capture with promotion already treated
        if (OP.has(~to)) {
            NonKingType captured{*OP.typeAt(~to)};
            if constexpr (Flags & WithZobrist) {
                zobrist_.opCapture(captured, ~to);
                prefetch();
            }

            OP.capture(~to);
            updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));

            if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to, captured); }
            return; // end of simple pawn capture move
        }

        if (from.on(Rank2) && to.on(Rank4)) {
            updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
            setLegalEnPassant<My>(to); //TRICK: updateSliderAttacks<My>() needed before
            if constexpr (Flags & WithZobrist) {
                if (MY.hasEnPassant()) { zobrist_.enPassant(MY.sqEnPassant()); }
                prefetch();
            }
        } else {
            if constexpr (Flags & WithZobrist) { prefetch(); }
            updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        }

        if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to); }
        return; // end of simple pawn push move
    }

    if (MY.isKing(from)) {
        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
            zobrist_.move(King, from, to);
        }
        MY.moveKing(from, to);
        OP.setOpKing(~to);

        if (OP.has(~to)) {
            NonKingType captured{*OP.typeAt(~to)};
            if constexpr (Flags & WithZobrist) {
                if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.opCapture(captured, ~to);
                prefetch();
            }

            rule50_.clear();
            OP.capture(~to);
            updateSliderAttacks<My>(MY.affectedBy(from));

            if constexpr (Flags & WithEval) { accumulator.move(King, from, to, captured); }
            return; // end of king capture move
        }

        if constexpr (Flags & WithZobrist) { prefetch(); }
        updateSliderAttacks<My>(MY.affectedBy(from, to));

        if constexpr (Flags & WithEval) { accumulator.move(King, from, to); }
        return; // end of king non-capture move
    }

    // castling move encoded as castling rook captures own king
    if (MY.isKing(to)) {
        Square rookFrom = from;
        Square kingFrom = to;
        Square kingTo = CastlingRules::castlingKingTo(kingFrom, rookFrom);
        Square rookTo = CastlingRules::castlingRookTo(kingFrom, rookFrom);

        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
            zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
            prefetch();
        }

        MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);
        OP.setOpKing(~kingTo);

        //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
        //TRICK: castling rook should attack 'kingFrom' square
        //TRICK: only first rank sliders can be affected
        updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.piecesOn(Rank1));

        if constexpr (Flags & WithEval) { accumulator.castle(kingFrom, kingTo, rookFrom, rookTo); }
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
        NonKingType captured{*OP.typeAt(~to)};
        if constexpr (Flags & WithZobrist) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
            prefetch();
        }

        rule50_.clear();
        OP.capture(~to);
        updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{pi}, OP.affectedBy(~from));

        if constexpr (Flags & WithEval) { accumulator.move(moved, from, to, captured); }
        return; // end of simple non-pawn non-king capture move
    }

    if constexpr (Flags & WithZobrist) { prefetch(); }
    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));

    if constexpr (Flags & WithEval) { accumulator.move(moved, from, to); }
    return; // end of simple non-pawn non-king quiet move
}

void Position::makeMove(const Position* parent, Square from, Square to, auto&& prefetch) {
    copyParent(parent);
    zobrist_ = parent->zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, Full>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}

void Position::makeMoveNoEval(const Position* parent, Square from, Square to, auto&& prefetch) {
    copyParent(parent);
    zobrist_ = parent->zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, WithZobrist>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}
