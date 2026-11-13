#include "PositionMoves.hpp"

template <Side::_t My>
void PositionMoves::generateEnPassantMoves() {
    constexpr Side::_t Op{~My};

    assert (_(My).hasEnPassant());
    assert (_(Op).hasEnPassant());

    File file = _(Op).fileEnPassant();
    assert (_(My).enPassantPawns() <= ( _(My).pawns() & _(My).attackersTo({file, Rank6}) ));
    for (Pi pi : _(My).enPassantPawns()) {
        moves_.add(pi, {file, Rank5});
    }
}

template <Side::_t My>
void PositionMoves::populateUnderpromotions() {
    for (Pi pi : _(My).promotables()) {
        // add underpromotions for each already generated legal queen promotion
        //TRICK: promoted piece type encoded inside pawn destination square rank
        Bb bb{moves_.bb(pi)};
        bb += bb.backward(); // Rook, Rank7
        bb |= bb.backward(); // Bishop, Rank6
        bb |= bb.backward(); // Knight, Rank5
        moves_.set(pi, bb);
    }
}

template <Side::_t My>
void PositionMoves::generateLegalKingMoves() {
    //TRICK: our attacks do not hide under attacked king shadow
    Bb kingMoves = ::attacksFrom(King, _(My).sqKing()) % (_(My).bbSide() | bbAttacked());
    moves_.set(PiKing, kingMoves);
}

template <Side::_t My>
void PositionMoves::generateCastlingMoves() {
    for (Pi pi : _(My).castlingRooks()) {
        if ( ::castlingRules.isLegal(_(My).sqKing(), _(My).sq(pi), bbBoth(My), bbAttacked()) ) {
            // castling encoded as the rook moves over the own king square
            moves_.add(pi, _(My).sqKing());
        }
    }
}

template <Side::_t My>
void PositionMoves::generatePawnMoves() {
    for (Pi pi : _(My).pawns()) {
        Square from{ _(My).sq(pi) };

        Bb bb{ Bb{from.forward()} % bbBoth(My) }; // push
        bb += (bb & Bb{Rank3}).forward() % bbBoth(My); // double push
        bb += ::attacksFrom(Pawn, from) & ~_(Op).bbSide(); // captures
        moves_.set(pi, bb);
    }
}

template <Side::_t My>
void PositionMoves::correctCheckEvasionsByPawns(Bb checkLine, Square checkFrom) {
    // simple pawn push over check line
    Bb potentialBlockers = checkLine.backward();

    // illegal phantom diagonal captures to fix
    Bb potentialInvalidCaptures = checkLine.backwardDiag();

    for (Square from : _(My).bbPawns() & (potentialBlockers | potentialInvalidCaptures)) {
        Bb bb = (Bb{from.forward()} & checkLine) + (::attacksFrom(Pawn, from) & Bb{checkFrom});
        moves_.set(_(My).pi(from), bb);
    }

    // pawns double push over check line
    Bb pawnJumpEvasions = _(My).bbPawns() & Bb{Rank2} & potentialBlockers.backward();
    pawnJumpEvasions %= bbBoth(My).backward(); // exlcude double push through occupied square
    for (Square from : pawnJumpEvasions) {
        moves_.add(_(My).pi(from), {from.file(), Rank4});
    }
}

// exclude illegal moves due pin
template <Side::_t My>
void PositionMoves::excludePinnedMoves(PiMask opPinners) {
    constexpr Side::_t Op{~My};

    for (Pi pinner : opPinners) {
        Square pinFrom{~_(Op).sq(pinner)};

        assert (::attacksFrom(_(Op).piece(pinner), pinFrom).has(_(My).sqKing()));

        Bb pinLine = ::inBetween(_(My).sqKing(), pinFrom);
        Bb occupiedPinLine = pinLine & bbBoth(My);
        assert (occupiedPinLine.isAny());

        if (occupiedPinLine.isSingleton() && occupiedPinLine.isAny(_(My).bbSide())) {
            // we discovered a true pinned piece
            Pi pinned = _(My).pi(occupiedPinLine.index());

            // exclude all pinned piece moves except those over the pin line
            moves_.filter(pinned, pinLine + Bb{pinFrom});
        }
    }
}

template <Side::_t My>
void PositionMoves::generateCheckEvasions() {
    constexpr Side::_t Op{~My};

    PiMask checkers = _(Op).checkers();

    if (!checkers.isSingleton()) {
        moves_ = {}; // double check case: no moves except king's ones are possible
    } else { // common single checker case
        Pi checker = checkers.pi();
        Square checkFrom{~_(Op).sq(checker)};
        Bb checkLine = ::inBetween(_(My).sqKing(), checkFrom);

        // check evasion moves of all pieces
        // (including invalid phantom pawn captures and missing pawn non-captures)
        moves_.setAttacks(_(My).attacks());
        moves_ &= checkLine + Bb{checkFrom};

        // pawns moves are special case
        correctCheckEvasionsByPawns<My>(checkLine, checkFrom);

        excludePinnedMoves<My>(_(Op).pinners() % checkers);

        populateUnderpromotions<My>();

        // trust out legal enpassant flag
        if (_(My).hasEnPassant()) { assert (_(Op).enPassantPawns() == checkers); generateEnPassantMoves<My>(); }
    }

    generateLegalKingMoves<My>();
}

// generate all legal moves from the current position for the current side to move
template <Side::_t My>
void PositionMoves::generateMoves() {
    constexpr Side::_t Op{~My};
    bbAttacked_ = ~_(Op).attacks().bb();

    inCheck_ = bbAttacked().has(_(My).sqKing());
    assert (_(Op).checkers().isAny() == bbAttacked().has(_(My).sqKing()));

    if (inCheck_) {
        generateCheckEvasions<My>();
        return;
    }

    // create pseudolegal moves from piece attacks
    // (including invalid phantom pawn captures and missing pawn non-captures)
    moves_.setAttacks(_(My).attacks());
    moves_ %= _(My).bbSide();

    // pawns moves treated separately
    generatePawnMoves<My>();

    //TRICK: castling encoded as a rook move, so we implicitly cover the case of pinned castling in Chess960
    generateCastlingMoves<My>();

    excludePinnedMoves<My>(_(Op).pinners());

    // encoding underpromotions: generate pseudomoves mirroring already generated queen promotions
    populateUnderpromotions<My>();

    // trust out legal enpassant flag
    if (_(My).hasEnPassant()) { generateEnPassantMoves<My>(); }

    generateLegalKingMoves<My>();
}

void PositionMoves::generateMoves() {
    generateMoves<My>();
    movesTotal_ = moves().popcount();
    movesMade_ = 0;
}
