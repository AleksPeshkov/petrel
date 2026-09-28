
#include <atomic>
#include "Position.hpp"

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected) {
    constexpr Side::_t Op{~My};

    occupied_[My] = _(My).bbSide() + ~_(Op).bbSide();
    occupied_[Op] = _(Op).bbSide() + ~_(My).bbSide();

    myAffected &= _(My).sliders();
    if (myAffected.isAny()) {
        _(My).updateSlidersCheckers(myAffected, bbBoth(My));
    }
}

template <Side::_t My>
void Position::updateSliderAttacks(PiMask myAffected, PiMask opAffected) {
    constexpr Side::_t Op{~My};

    updateSliderAttacks<My>(myAffected);

    opAffected &= _(Op).sliders();
    if (opAffected.isAny()) {
        _(Op).updateSliders(opAffected, bbBoth(Op));
    }
}

template <Side::_t My>
void Position::setLegalEnPassant(Square ep) {
    constexpr Side::_t Op{~My};

    assert (ep.isOn(Rank4));
    assert (_(My).isPawn(ep));
    assert (!_(My).hasEnPassant());
    assert (!_(Op).hasEnPassant());

    Square to{ep.file(), Rank3}; // attacking pawn destination square

    // check if there are any pawns to capture ep victim
    Bb bbKillers{~_(Op).bbPawns() & ::attacksFrom(Pawn, to)};
    if (bbKillers.isNone()) { return; }

    // discovered check
    if (_(My).isKingDiscovered(bbBoth(My))) { assert ((_(My).checkers() % PiMask{_(My).pi(ep)}).isAny()); return; }
    assert ((_(My).checkers() % PiMask{_(My).pi(ep)}).isNone());

    for (Square from : bbKillers) {
        assert (from.isOn(Rank4));

        if (!_(My).isKingDiscovered(bbBoth(My).move(from, to) - Bb{ep})) {
            _(My).setEnPassantVictim(ep);
            _(Op).setEnPassantKiller(~from);
        }
    }
}

inline void DualAcc::setup(const Position& pos) {
    mirror[My] = pos._(My).sqKing().mirrorMask();
    side[My].setup<My>(pos, mirror[My]);

    mirror[Op] = pos._(Op).sqKing().mirrorMask();
    side[Op].setup<Op>(pos, mirror[Op]);
}

struct PiecesIndex : Index<PiecesIndex, 2*Pi::size()> { using Index::Index; };

template <Side::_t AccMy>
inline void Acc::setup(const Position& pos, Square mirror) {
    assert (pos._(AccMy).sqKing().mirrorMask() == mirror);

    int count{0};
    array<Fi, PiecesIndex> fi;

    auto& my{ pos._(AccMy) };
    for (Pi pi : my.any()) {
        fi[PiecesIndex{count++}] = {My, my.piece(pi), my.sq(pi)^mirror};
    }

    //TRICK: flip pieces squares perspective for opposite side
    auto op_mirror = ~mirror;
    auto& op{ pos._(~AccMy) };
    for (Pi pi : op.any()) {
        fi[PiecesIndex{count++}] = {Op, op.piece(pi), op.sq(pi)^op_mirror};
    }

    for (auto n : range<AccIndex>()) {
        _t a{};
        for (int i = 0; i < count; ++i) {
            a = adds_i16(a, nnue.w0[ fi[PiecesIndex{i}]][n] );
        }
        acc[n] = a;
    }
}

constexpr void DualAcc::moveKing(const Position& pos, Square from, Square to) {
    assert (from != to);
    if (+(from ^ to) & 4) {
        // king crossed the horizontal middle line
        mirror[Op] = mirror[Op].mirror();
        side[Op].setup<Op>(pos, mirror[Op]);
    } else {
        side[Op].move(mirror[Op], My, King, from, to);
    }
    side[My].move(~mirror[My], Op, King, from, to);
}

constexpr void DualAcc::moveKing(const Position& pos, Square from, Square to, NonKingPiece captured) {
    assert (from != to);
    if (+(from ^ to) & 4) {
        // king crossed the horizontal middle line
        mirror[Op] = mirror[Op].mirror();
        side[Op].setup<Op>(pos, mirror[Op]);
    } else {
        side[Op].move(mirror[Op], My, King, from, to, captured);
    }
    side[My].move(~mirror[My], Op, King, from, to, captured);
}

constexpr void DualAcc::castle(const Position& pos, Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
    assert (kingFrom != rookFrom); assert (kingTo != rookTo);
    assert (kingFrom.isOn(Rank1)); assert (rookTo.isOn(Rank1));
    if (+(kingFrom ^ kingTo) & 4) {
        // king crossed the horizontal middle line
        mirror[Op] = mirror[Op].mirror();
        side[Op].setup<Op>(pos, mirror[Op]);
    } else {
        side[Op].castle(mirror[Op], My, kingFrom, kingTo, rookFrom, rookTo);
    }
    side[My].castle(~mirror[My], Op, kingFrom, kingTo, rookFrom, rookTo);
}

template <Side::_t My, Position::make_move_flags_enum Flags>
bool Position::makeMove(Square from, Square to, auto&& flipPrefetch) {
    constexpr Side::_t Op{~My};

    // assumes that the given move is valid and legal
    assert (_(My).checkers().isNone());
    _(Op).clearCheckers();

    if (_(Op).hasEnPassant()) [[unlikely]] {
        if constexpr (Flags & WithZobrist) { zobrist_.opEnPassant(_(Op).sqEnPassant()); }
        _(My).clearEnPassantKillers(); // can be two

        if (_(My).isPawn(from) && from.isOn(Rank5) && to.isOn(Rank5)) [[unlikely]] {
            // en passant capture encoded as the pawn captures the pawn
            Square ep{to};
            to = {to.file(), Rank6};

            if constexpr (Flags & WithZobrist) {
                zobrist_.move(Pawn, from, to);
                zobrist_.opCapture(Pawn, ~ep);
                flipPrefetch();
                rule50_ = {}; zHash_ = {}; // ep capture resets rule50
            }

            _(Op).capture(Pawn, ~ep); //TRICK: also clears en passant victim
            _(My).movePawn(from, to);
            updateSliderAttacks<My>(_(My).affectedBy(from, to, ep), _(Op).affectedBy(~from, ~to, ~ep));
            if constexpr (Flags & WithEval) { accumulator.ep(from, to, ep); }
            return true; // end of en passant capture move
        }

        // clear en passant status from the previous move
        _(Op).clearEnPassantVictim();
    }
    assert (!_(Op).hasEnPassant());
    assert (!_(My).hasEnPassant());

    if (_(My).isPawn(from)) [[unlikely]] {
        if constexpr (Flags & WithZobrist) {
            rule50_ = {}; zHash_ = {}; // any pawn move resets rule50
        }

        if (!from.isOn(Rank7)) {
            // simple pawn capture or noncapture, cannot be en passant capture
            if constexpr (Flags & WithZobrist) { zobrist_.move(Pawn, from, to); }

            if (_(Op).has(~to)) {
                NonKingPiece captured{*_(Op).pieceAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                _(Op).capture(captured, ~to);
                _(My).movePawn(from, to);
                updateSliderAttacks<My>(_(My).affectedBy(from), _(Op).affectedBy(~from));
                if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to, captured); }
                return true; // end of simple pawn capture move
            } else {
                if (from.isOn(Rank2) && to.isOn(Rank4)) {
                    _(My).movePawn(from, to);
                    updateSliderAttacks<My>(_(My).affectedBy(from, to), _(Op).affectedBy(~from, ~to));
                    setLegalEnPassant<My>(to); //TRICK: updateSliderAttacks<My>() needed before
                    if constexpr (Flags & WithZobrist) {
                        if (_(My).hasEnPassant()) [[unlikely]] {
                            zobrist_.enPassant(_(My).sqEnPassant());
                        }
                        flipPrefetch();
                    }
                } else {
                    if constexpr (Flags & WithZobrist) { flipPrefetch(); }
                    _(My).movePawn(from, to);
                    updateSliderAttacks<My>(_(My).affectedBy(from, to), _(Op).affectedBy(~from, ~to));
                }
                if constexpr (Flags & WithEval) { accumulator.move(Pawn, from, to); }
                return true; // end of simple pawn push move
            }
        } else [[unlikely]] {
            // pawn promotion

            Officer officer{ ::officerFrom(to.rank()) }; // decode promoted piece
            to = {to.file(), Rank8}; //TRICK: correct move destination square
            if constexpr (Flags & WithZobrist) { zobrist_.promote(from, officer, to); }

            if (_(Op).has(~to)) [[unlikely]] {
                NonKingPiece captured{*_(Op).pieceAt(~to)};
                if constexpr (Flags & WithZobrist) {
                    if (_(Op).isCastling(~to)) [[unlikely]] { zobrist_.opCastling(~to); } // captured the rook with castling right
                    zobrist_.opCapture(captured, ~to);
                    flipPrefetch();
                }

                _(Op).capture(captured, ~to);
                PiMask promoted{ _(My).piPromoted(from, officer, to) }; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(_(My).affectedBy(from) | promoted, _(Op).affectedBy(~from));
                if constexpr (Flags & WithEval) { accumulator.promote(from, officer, to, captured); }
                return true; // end of pawn promotion move with capture
            } else {
                if constexpr (Flags & WithZobrist) { flipPrefetch(); }

                PiMask promoted{ _(My).piPromoted(from, officer, to) }; // promoted piece index can differ from pawn piece index
                updateSliderAttacks<My>(_(My).affectedBy(from, to) | promoted, _(Op).affectedBy(~from, ~to));
                if constexpr (Flags & WithEval) { accumulator.promote(from, officer, to); }
                return true; // end of pawn promotion move without capture
            }
        } // promotion or not
    } // no pawn moves anymore

    if (_(My).isKing(from)) [[unlikely]] {
        // king move is special case as it affects castling rights
        bool shouldResetZHash = false;
        if constexpr (Flags & WithZobrist) {
            for (Pi piRook : _(My).castlingRooks()) [[unlikely]] {
                zobrist_.castling(_(My).sq(piRook));
                zHash_ = {}; shouldResetZHash = true; // king move changed castling rights
            }
            zobrist_.move(King, from, to);
        }

        if (_(Op).has(~to)) {
            NonKingPiece captured{*_(Op).pieceAt(~to)};
            if constexpr (Flags & WithZobrist) {
                if (_(Op).isCastling(~to)) [[unlikely]] { zobrist_.opCastling(~to); } // captured the rook with castling right
                zobrist_.opCapture(captured, ~to);
                flipPrefetch();
                rule50_ = {}; zHash_ = {}; // capture resets rule50
            }

            _(Op).capture(captured, ~to);
            _(Op).setOpKing(~to);
            _(My).move(PiKing, from, to);
            _(My).updateMovedKing(to);
            updateSliderAttacks<My>(_(My).affectedBy(from)); // king cannot affect enemy attacks
            if constexpr (Flags & WithEval) { accumulator.moveKing(*this, from, to, captured); }
            return true; // end of king capture move
        } else {
            if constexpr (Flags & WithZobrist) {
                flipPrefetch();
                rule50_.next(); // zHash_ kept unless king move affected castling rights
            }

            _(My).move(PiKing, from, to);
            _(My).updateMovedKing(to);
            _(Op).setOpKing(~to);
            updateSliderAttacks<My>(_(My).affectedBy(from, to)); // king cannot affect enemy attacks
            if constexpr (Flags & WithEval) { accumulator.moveKing(*this, from, to); }
            return shouldResetZHash; // end of king non-capture move
        }
    } // no king moves anymore

// non-pawn non-king move (but can be castling):
    Pi pi = _(My).pi(from);
    bool shouldResetZHash = false;

    if (_(My).isCastling(pi)) [[unlikely]] {
        if (_(My).isKing(to)) [[likely]] {
            // castling move encoded as castling rook captures own king
            Square rookFrom{from};
            Square kingFrom{to};
            Square kingTo{CastlingRules::castlingKingTo(kingFrom, rookFrom)};
            Square rookTo{CastlingRules::castlingRookTo(kingFrom, rookFrom)};

            if constexpr (Flags & WithZobrist) {
                for (Pi piRook : _(My).castlingRooks()) [[likely]] { zobrist_.castling(_(My).sq(piRook)); }
                zobrist_.castle(kingFrom, kingTo, rookFrom, rookTo);
                flipPrefetch();
                rule50_.next(); zHash_ = {}; // castling holds rule50, but not ZHash
            }

            _(Op).setOpKing(~kingTo);
            _(My).castle(kingFrom, kingTo, pi, rookFrom, rookTo);

            //TRICK: castling should not affect opponent's sliders, otherwise it is check or pin
            //TRICK: castling rook should attack 'kingFrom' square
            //TRICK: only first rank sliders can be affected
            updateSliderAttacks<My>(_(My).affectedBy(rookFrom, kingFrom) & _(My).any(Rank1));
            if constexpr (Flags & WithEval) { accumulator.castle(*this, kingFrom, kingTo, rookFrom, rookTo); }
            return true; // end of castling move
        }

        if constexpr (Flags & WithZobrist) {
            // move of the rook with castling right
            zobrist_.castling(from);
            zHash_ = {}; shouldResetZHash = true; // changed castling right
        }
    }

    Officer officer{*_(My).piece(pi)}; // officers: Q, R, B, N
    if constexpr (Flags & WithZobrist) { zobrist_.move(officer, from, to); }

    if (_(Op).has(~to)) {
        NonKingPiece captured{*_(Op).pieceAt(~to)};
        if constexpr (Flags & WithZobrist) {
            if (_(Op).isCastling(~to)) { zobrist_.opCastling(~to); } // captured the rook with castling right
            zobrist_.opCapture(captured, ~to);
            flipPrefetch();
            rule50_ = {}; zHash_ = {}; // capture resets rule50
        }

        _(Op).capture(captured, ~to);
        _(My).move(pi, officer, from, to);
        updateSliderAttacks<My>(_(My).affectedBy(from) | PiMask{pi}, _(Op).affectedBy(~from));
        if constexpr (Flags & WithEval) { accumulator.move(officer, from, to, captured); }
        return true; // end of officer's capture
    } else {
        if constexpr (Flags & WithZobrist) {
            flipPrefetch();
            rule50_.next(); // zHash_ kept, unless moved rook with castling right
        }

        _(My).move(pi, officer, from, to);
        updateSliderAttacks<My>(_(My).affectedBy(from, to), _(Op).affectedBy(~from, ~to));
        if constexpr (Flags & WithEval) { accumulator.move(officer, from, to); }
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
