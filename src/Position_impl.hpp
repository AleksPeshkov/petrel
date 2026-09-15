
#include <atomic>
#include "Position.hpp"

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected) {
    constexpr Side::_t Op{~My};

    occupied_[My] = MY.bbSide() + ~OP.bbSide();
    occupied_[Op] = OP.bbSide() + ~MY.bbSide();

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

inline void DualAcc::setup(const Position& pos) {
    sqFlip[My] = pos.positionSide(My).sqKing().hMask();
    dacc[My].setup<My>(pos, sqFlip[My]);

    sqFlip[Op] = pos.positionSide(Op).sqKing().hMask();
    dacc[Op].setup<Op>(pos, sqFlip[Op]);
}

struct PiecesIndex : Index<PiecesIndex, 2*Pi::size()> { using Index::Index; };

template <Side::_t AccMy>
inline void Acc::setup(const Position& pos, Square sqFlip) {
    assert (pos.positionSide(AccMy).sqKing().hMask() == sqFlip);

    int count{0};
    array<Fi, PiecesIndex> fi;

    auto& my{ pos.positionSide(AccMy) };
    for (auto pi : my.any()) {
        fi[PiecesIndex{count++}] = {My, my.piece(pi), my.sq(pi)^sqFlip};
    }

    //TRICK: flip pieces squares perspective for opposite side
    auto op_sqFlip = ~sqFlip;
    auto& op{ pos.positionSide(~AccMy) };
    for (auto pi : op.any()) {
        fi[PiecesIndex{count++}] = {Op, op.piece(pi), op.sq(pi)^op_sqFlip};
    }

    for (auto n : range<Index>()) {
        Nnue::_t a{};
        for (int i = 0; i < count; ++i) {
            a = adds_i16(a, nnue.w0[ fi[PiecesIndex{i}]][n] );
        }
        acc[n] = a;
    }
}

constexpr void DualAcc::moveKing(const Position& pos, Square from, Square to) {
    assert (from != to);
    if (Square::crossed_middle(from, to)) {
        sqFlip[Op] = sqFlip[Op].hm();
        dacc[Op].setup<Op>(pos, sqFlip[Op]);
    } else {
        dacc[Op].move(sqFlip[Op], My, King, from, to);
    }
    dacc[My].move(~sqFlip[My], Op, King, from, to);
}

constexpr void DualAcc::moveKing(const Position& pos, Square from, Square to, NonKingPiece captured) {
    assert (from != to);
    if (Square::crossed_middle(from, to)) {
        sqFlip[Op] = sqFlip[Op].hm();
        dacc[Op].setup<Op>(pos, sqFlip[Op]);
    } else {
        dacc[Op].move(sqFlip[Op], My, King, from, to, captured);
    }
    dacc[My].move(~sqFlip[My], Op, King, from, to, captured);
}

constexpr void DualAcc::castle(const Position& pos, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
    assert (kingFrom != rookFrom); assert (kingTo != rookTo);
    assert (kingFrom.on(Rank1)); assert (rookTo.on(Rank1));
    if (Square::crossed_middle(kingFrom, kingTo)) {
        sqFlip[Op] = sqFlip[Op].hm();
        dacc[Op].setup<Op>(pos, sqFlip[Op]);
    } else {
        dacc[Op].castle(sqFlip[Op], My, kingFrom, kingTo, rookFrom, rookTo);
    }
    dacc[My].castle(~sqFlip[My], Op, kingFrom, kingTo, rookFrom, rookTo);
}

template <Side::_t My, Position::make_move_flags_enum Flags>
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
                zobrist_.opCapture(NonKingPiece{Pawn}, ~ep);
                flipPrefetch();
                rule50_ = {}; zHash_ = {}; // ep capture resets rule50
            }

            OP.capture(~ep); //TRICK: also clears en passant victim
            MY.clearEnPassantKillers(); // can be two
            MY.movePawn(from, to);
            updateSliderAttacks<My>(MY.affectedBy(from, to, ep), OP.affectedBy(~from, ~to, ~ep));
            if constexpr (Flags & WithEval) { dacc.ep(from, to, ep); }
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
                NonKingPiece captured{*OP.pieceAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                OP.capture(~to);
                MY.movePawn(from, to);
                updateSliderAttacks<My>(MY.affectedBy(from), OP.affectedBy(~from));
                if constexpr (Flags & WithEval) { dacc.move(Pawn, from, to, captured); }
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
                if constexpr (Flags & WithEval) { dacc.move(Pawn, from, to); }
                return true; // end of simple pawn push move
            }
        } else [[unlikely]] {
            // pawn promotion

            Officer officer{ ::officerFrom(to.rank()) }; // decode promoted piece
            to = {to.file(), Rank8}; //TRICK: correct move destination square
            if constexpr (Flags & WithZobrist) { zobrist_.promote(from, officer, to); }

            if (OP.has(~to)) [[unlikely]] {
                NonKingPiece captured{*OP.pieceAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    if (OP.isCastling(~to)) [[unlikely]] { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                OP.capture(~to);
                PiMask promoted{ MY.piPromoted(from, officer, to) }; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(MY.affectedBy(from) | promoted, OP.affectedBy(~from));
                if constexpr (Flags & WithEval) { dacc.promote(from, officer, to, captured); }
                return true; // end of pawn promotion move with capture
            } else {
                if constexpr (Flags & WithZobrist) { flipPrefetch(); }

                PiMask promoted{ MY.piPromoted(from, officer, to) }; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(MY.affectedBy(from, to) | promoted, OP.affectedBy(~from, ~to));
                if constexpr (Flags & WithEval) { dacc.promote(from, officer, to); }
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
            NonKingPiece captured{*OP.pieceAt(~to)};
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
            if constexpr (Flags & WithEval) { dacc.moveKing(*this, from, to, captured); }
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
            if constexpr (Flags & WithEval) { dacc.moveKing(*this, from, to); }
            return shouldResetZHash; // end of king non-capture move
        }
    } // no king moves anymore

// non-pawn non-king move (but can be castling):
    Pi pi{ MY.pi(from) };
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
            if constexpr (Flags & WithEval) { dacc.castle(*this, kingFrom, kingTo, rookFrom, rookTo); }
            return true; // end of castling move
        }

        if constexpr (Flags & WithZobrist) {
            // move of the rook with castling right
            zobrist_.castling(from);
            zHash_ = {}; shouldResetZHash = true; // changed castling right
        }
    }

    Officer officer{*MY.piece(pi)}; // officers: Q, R, B, N
    if constexpr (Flags & WithZobrist) { zobrist_.move(officer, from, to); }

    if (OP.has(~to)) {
        NonKingPiece captured{*OP.pieceAt(~to)};
        if constexpr (Flags & WithZobrist) {
            if (OP.isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
            flipPrefetch();
            rule50_ = {}; zHash_ = {}; // capture resets rule50
        }

        OP.capture(~to);
        MY.move(pi, officer, from, to);
        updateSliderAttacks<My>(MY.affectedBy(from) | PiMask{pi}, OP.affectedBy(~from));
        if constexpr (Flags & WithEval) { dacc.move(officer, from, to, captured); }
        return true; // end of officer's capture
    } else {
        if constexpr (Flags & WithZobrist) {
            flipPrefetch();
            rule50_.next(); // zHash_ kept, unless moved rook with castling right
        }

        MY.move(pi, officer, from, to);
        updateSliderAttacks<My>(MY.affectedBy(from, to), OP.affectedBy(~from, ~to));
        if constexpr (Flags & WithEval) { dacc.move(officer, from, to); }
        return shouldResetZHash; // end of officers's noncapture move
    }
}

bool Position::makeMove(const Position& parent, Square from, Square to, ZHash zHash, auto&& prefetch) {
    copy_swap(parent);
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
    copy_swap(parent);
    //zHash_ = {}; shouldResetZHash_ = false;// unused
    zobrist_ = parent.zobrist_;

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, NoEval>(from, to, [&]{ zobrist_.flip(); prefetch(z()); });

    //assert (z() == generateZobrist().v()); // true, but slow to compute
}
