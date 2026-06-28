
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
    Bb killers{~OP.bbPawns() & ::attacksFrom(Pawn, to)};
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

    // assumes that the given move is valid and legal
    assert (MY.checkers().none());
    OP.clearCheckers();

    // clear en passant status from the previous move
    if (OP.hasEnPassant()) {
        if constexpr (Flags & WithZobrist) { zobrist_.opEnPassant(OP.sqEnPassant()); }
        MY.clearEnPassantKillers(); // can be two

        // en passant capture encoded as the pawn captures the pawn
        if (MY.isPawn(from) && from.on(Rank5) && to.on(Rank5)) {
            Square ep{to};
            to = Square{to.file(), Rank6};

            if constexpr (Flags & WithZobrist) {
                zobrist_.move(Pawn, from, to);
                zobrist_.opCapture(NonKingType{Pawn}, ~ep);
                prefetch();
            }

            MY.movePawn(from, to);

            OP.capture(~ep); // also clears en passant victim
            rule50_ = {};
            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));
            if constexpr (Flags & WithEval) { accumulator.ep(from, to, ep); }
            return; // end of en passant capture move
        }

        OP.clearEnPassantVictim();
    }

    assert (!MY.hasEnPassant()); assert (!OP.hasEnPassant());

    if (MY.isPawn(from)) {
        rule50_ = {}; // any pawn move resets rule50

        if (from.on(Rank7)) {
            PromoType promoType{::promoTypeFrom(to.rank())}; // decode promoted piece
            to = {to.file(), Rank8}; // correct move destination square

            if constexpr (Flags & WithZobrist) { zobrist_.promote(from, promoType, to); }
            // promoted piece index can differ from pawn piece index
            Pi promoted{MY.piPromoted(from, promoType, to)};

            if (OP.has(~to)) {
                NonKingType captured{*OP.typeAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                    prefetch();
                }

                OP.capture(~to);
                updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{promoted}, OP.affectedBy(~from));
                if constexpr (Flags & WithEval) { accumulator.promote(from, promoType, to, captured); }
                return; // end of pawn promotion move with capture
            }

            if constexpr (Flags & WithZobrist) { prefetch(); }
            updateSliderAttacks<My>(MY.affectedBy(from, to) | PiMask{promoted}, OP.affectedBy(~from, ~to));
            if constexpr (Flags & WithEval) { accumulator.promote(from, promoType, to); }
            return; // end of pawn promotion move without capture
        }

        if constexpr (Flags & WithZobrist) { zobrist_.move(Pawn, from, to); }
        MY.movePawn(from, to);

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

        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if (from.on(Rank2) && to.on(Rank4)) {
            setLegalEnPassant<My>(to); //TRICK: updateSliderAttacks<My>() needed before
            if constexpr (Flags & WithZobrist) {
                if (MY.hasEnPassant()) { zobrist_.enPassant(MY.sqEnPassant()); }
            }
        }
        if constexpr (Flags & WithZobrist) { prefetch(); }
        if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to); }
        return; // end of simple pawn push move
    }
// no pawn moves anymore

    // king move is special case
    if (MY.isKing(from)) {
        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
            zobrist_.move(King, from, to);
        }
        MY.move(Pi{TheKing}, from, to);
        MY.updateMovedKing(to);
        OP.setOpKing(~to);

        if (OP.has(~to)) {
            NonKingType captured{*OP.typeAt(~to)};
            if constexpr (Flags & WithZobrist) {
                if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.opCapture(captured, ~to);
                prefetch();
            }

            OP.capture(~to);
            rule50_ = {};
            updateSliderAttacks<My>(MY.affectedBy(from)); // king cannot affect enemy attacks
            if constexpr (Flags & WithEval) { accumulator.move(King, from, to, captured); }
            return; // end of king capture move
        }

        rule50_.next();
        if constexpr (Flags & WithZobrist) { prefetch(); }
        updateSliderAttacks<My>(MY.affectedBy(from, to)); // king cannot affect enemy attacks
        if constexpr (Flags & WithEval) { accumulator.move(King, from, to); }
        return; // end of king non-capture move
    }

// non-pawn non-king move (but can be castling):
    Pi pi = MY.pi(from);
    PromoType promoType{*MY.typeOf(pi)};

    // move of the rook with castling right
    if (MY.isCastling(pi)) {
        // castling move encoded as castling rook captures own king
        if (MY.isKing(to)) {
            Square rookFrom{from};
            Square kingFrom{to};
            Square kingTo{CastlingRules::castlingKingTo(kingFrom, rookFrom)};
            Square rookTo{CastlingRules::castlingRookTo(kingFrom, rookFrom)};

            if constexpr (Flags & WithZobrist) {
                for (Pi rook : MY.castlingRooks()) { zobrist_.castling(MY.sq(rook)); }
                zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
                prefetch();
            }

            MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);
            OP.setOpKing(~kingTo);

            rule50_.next();
            //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
            //TRICK: castling rook should attack 'kingFrom' square
            //TRICK: only first rank sliders can be affected
            updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.anyOn(Rank1));
            if constexpr (Flags & WithEval) { accumulator.castle(kingFrom, kingTo, rookFrom, rookTo); }
            return; // end of castling move
        }

        if constexpr (Flags & WithZobrist) { zobrist_.castling(from); } // clear just moved rook castling right
    }

    if constexpr (Flags & WithZobrist) { zobrist_.move(promoType, from, to); }
    MY.move(pi, promoType, from, to);

    if (OP.has(~to)) {
        NonKingType captured{*OP.typeAt(~to)};
        if constexpr (Flags & WithZobrist) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
            prefetch();
        }

        OP.capture(~to);
        rule50_ = {};
        updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{pi}, OP.affectedBy(~from));
        if constexpr (Flags & WithEval) { accumulator.move(promoType, from, to, captured); }
        return; // end of simple capture
    }

    rule50_.next();
    if constexpr (Flags & WithZobrist) { prefetch(); }
    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
    if constexpr (Flags & WithEval) { accumulator.move(promoType, from, to); }
    return; // end of simple quiet move
}

void Position::makeMove(const Position& parent, Square from, Square to, auto&& prefetch) {
    flip(parent);
    zobrist_ = parent.zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, Full>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}

void Position::makeMoveNoEval(const Position& parent, Square from, Square to, auto&& prefetch) {
    flip(parent);
    zobrist_ = parent.zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, NoEval>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}
