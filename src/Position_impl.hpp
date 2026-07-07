
#include <atomic>
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
bool Position::makeMove(Square from, Square to, auto&& flipPrefetch) {
    constexpr Side::_t Op{~My};

    // assumes that the given move is valid and legal
    assert (MY.checkers().none());
    OP.clearCheckers();

    if (OP.hasEnPassant()) [[unlikely]] {
        if (MY.isPawn(from) && from.on(Rank5) && to.on(Rank5)) [[unlikely]] {
            // en passant capture encoded as the pawn captures the pawn
            Square ep{to};
            to = Square{to.file(), Rank6};

            if constexpr (Flags & WithZobrist) {
                zobrist_.opEnPassant(OP.sqEnPassant());
                zobrist_.move(Pawn, from, to);
                zobrist_.opCapture(NonKingType{Pawn}, ~ep);
                flipPrefetch();
                rule50_ = {}; zHash_ = {}; // ep capture resets rule50
            }

            OP.capture(~ep); //TRICK: also clears en passant victim
            MY.clearEnPassantKillers(); // can be two
            MY.movePawn(from, to);
            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));
            if constexpr (Flags & WithEval) { accumulator.ep(from, to, ep); }
            return true; // end of en passant capture move
        }

        // clear en passant status from the previous move
        if constexpr (Flags & WithZobrist) { zobrist_.opEnPassant(OP.sqEnPassant()); }
        MY.clearEnPassantKillers(); // can be two
        OP.clearEnPassantVictim();
    }
    assert (!OP.hasEnPassant());
    assert (!MY.hasEnPassant());

    if (MY.isPawn(from)) [[unlikely]] {
        if constexpr (Flags & WithZobrist) {
            rule50_ = {}; zHash_ = {}; // any pawn move resets rule50
        }

        if (!from.on(Rank7)) {
            // simple pawn capture or noncapture, cannot be en passant capture
            if constexpr (Flags & WithZobrist) { zobrist_.move(Pawn, from, to); }

            if (OP.has(~to)) {
                NonKingType captured{*OP.typeAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                OP.capture(~to);
                MY.movePawn(from, to);
                updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));
                if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to, captured); }
                return true; // end of simple pawn capture move
            } else {
                if (from.on(Rank2) && to.on(Rank4)) {
                    MY.movePawn(from, to);
                    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
                    setLegalEnPassant<My>(to); //TRICK: updateSliderAttacks<My>() needed before
                    if constexpr (Flags & WithZobrist) {
                        if (MY.hasEnPassant()) [[unlikely]] {
                            zobrist_.enPassant(MY.sqEnPassant());
                        }
                        flipPrefetch();
                    }
                } else {
                    if constexpr (Flags & WithZobrist) { flipPrefetch(); }
                    MY.movePawn(from, to);
                    updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
                }
                if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to); }
                return true; // end of simple pawn push move
            }
        } else [[unlikely]] {
            // pawn promotion

            PromoType promoType{::promoTypeFrom(to.rank())}; // decode promoted piece
            to = {to.file(), Rank8}; //TRICK: correct move destination square
            if constexpr (Flags & WithZobrist) { zobrist_.promote(from, promoType, to); }

            if (OP.has(~to)) [[unlikely]] {
                NonKingType captured{*OP.typeAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    if (OP.isCastling(~to)) [[unlikely]] { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                OP.capture(~to);
                Pi promoted{MY.piPromoted(from, promoType, to)}; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{promoted}, OP.affectedBy(~from));
                if constexpr (Flags & WithEval) { accumulator.promote(from, promoType, to, captured); }
                return true; // end of pawn promotion move with capture
            } else {
                if constexpr (Flags & WithZobrist) { flipPrefetch(); }

                Pi promoted{MY.piPromoted(from, promoType, to)}; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(MY.affectedBy(from, to) | PiMask{promoted}, OP.affectedBy(~from, ~to));
                if constexpr (Flags & WithEval) { accumulator.promote(from, promoType, to); }
                return true; // end of pawn promotion move without capture
            }
        } // promotion or not
    } // no pawn moves anymore

    if (MY.isKing(from)) [[unlikely]] {
        // king move is special case as it affects castling rights
        bool shouldResetZHash = false;
        if constexpr (Flags & WithZobrist) {
            for (Pi rook : MY.castlingRooks()) [[unlikely]] {
                zobrist_.castling(MY.sq(rook));
                zHash_ = {}; shouldResetZHash = true; // king move changed castling rights
            }
            zobrist_.move(King, from, to);
        }

        if (OP.has(~to)) {
            NonKingType captured{*OP.typeAt(~to)};
            if constexpr (Flags & WithZobrist) {
                if (OP.isCastling(~to)) [[unlikely]] { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.opCapture(captured, ~to);
                flipPrefetch();
                rule50_ = {}; zHash_ = {}; // capture resets rule50
            }

            OP.capture(~to);
            OP.setOpKing(~to);
            MY.move(Pi{TheKing}, from, to);
            MY.updateMovedKing(to);
            updateSliderAttacks<My>(MY.affectedBy(from)); // king cannot affect enemy attacks
            if constexpr (Flags & WithEval) { accumulator.move(King, from, to, captured); }
            return true; // end of king capture move
        } else {
            if constexpr (Flags & WithZobrist) {
                flipPrefetch();
                rule50_.next(); // zHash_ kept unless king move affected castling rights
            }

            MY.move(Pi{TheKing}, from, to);
            MY.updateMovedKing(to);
            OP.setOpKing(~to);
            updateSliderAttacks<My>(MY.affectedBy(from, to)); // king cannot affect enemy attacks
            if constexpr (Flags & WithEval) { accumulator.move(King, from, to); }
            return shouldResetZHash; // end of king non-capture move
        }
    } // no king moves anymore

// non-pawn non-king move (but can be castling):
    Pi pi = MY.pi(from);
    bool shouldResetZHash = false;

    if (MY.isCastling(pi)) [[unlikely]] {
        if (MY.isKing(to)) [[likely]] {
            // castling move encoded as castling rook captures own king
            Square rookFrom{from};
            Square kingFrom{to};
            Square kingTo{CastlingRules::castlingKingTo(kingFrom, rookFrom)};
            Square rookTo{CastlingRules::castlingRookTo(kingFrom, rookFrom)};

            if constexpr (Flags & WithZobrist) {
                for (Pi rook : MY.castlingRooks()) [[likely]] { zobrist_.castling(MY.sq(rook)); }
                zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
                flipPrefetch();
                rule50_.next(); zHash_ = {}; // castling holds rule50, but not ZHash
            }

            OP.setOpKing(~kingTo);
            MY.castle(kingFrom, kingTo, pi, rookFrom, rookTo);

            //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
            //TRICK: castling rook should attack 'kingFrom' square
            //TRICK: only first rank sliders can be affected
            updateSliderAttacks<My>(MY.affectedBy(rookFrom, kingFrom) & MY.anyOn(Rank1));
            if constexpr (Flags & WithEval) { accumulator.castle(kingFrom, kingTo, rookFrom, rookTo); }
            return true; // end of castling move
        }

        if constexpr (Flags & WithZobrist) {
            // move of the rook with castling right
            zobrist_.castling(from);
            zHash_ = {}; shouldResetZHash = true; // changed castling right
        }
    }

    PromoType promoType{*MY.typeOf(pi)}; // officers: Q, R, B, N
    if constexpr (Flags & WithZobrist) { zobrist_.move(promoType, from, to); }

    if (OP.has(~to)) {
        NonKingType captured{*OP.typeAt(~to)};
        if constexpr (Flags & WithZobrist) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
            flipPrefetch();
            rule50_ = {}; zHash_ = {}; // capture resets rule50
        }

        OP.capture(~to);
        MY.move(pi, promoType, from, to);
        updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{pi}, OP.affectedBy(~from));
        if constexpr (Flags & WithEval) { accumulator.move(promoType, from, to, captured); }
        return true; // end of officer's capture
    } else {
        if constexpr (Flags & WithZobrist) {
            flipPrefetch();
            rule50_.next(); // zHash_ kept, unless moved rook with castling right
        }

        MY.move(pi, promoType, from, to);
        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if constexpr (Flags & WithEval) { accumulator.move(promoType, from, to); }
        return shouldResetZHash; // end of officers's noncapture move
    }
}

bool Position::makeMove(const Position& parent, Square from, Square to, ZHash zHash, auto&& prefetch) {
    flip(parent);
    zHash_ = zHash;
    zobrist_ = parent.zobrist_;

    auto flipPrefetch = [&]{
        zobrist_.flip();
        prefetch(z());
        //TRICK: force prefetch as early as possible
        std::atomic_signal_fence(std::memory_order_acquire);
    };

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    bool shouldResetZHash = makeMove<Op, Full>(from, to, flipPrefetch);
    //assert (z() == generateZobrist().v()); // true, but slow to compute

    prefetch(z()); // prefetch again after NNUE update
    return shouldResetZHash;
}

void Position::makeMovePerft(const Position& parent, Square from, Square to, auto&& prefetch) {
    flip(parent);
    //zHash_ = {}; shouldResetZHash_ = false;// unused
    zobrist_ = parent.zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, NoEval>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}
