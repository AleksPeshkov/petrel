#include "PositionMoves.hpp"

template <Side::_t My>
void PositionMoves::generateEnPassantMoves() {
    constexpr Side::_t Op{~My};

    assert (MY.hasEnPassant());
    assert (OP.hasEnPassant());

    File file = OP.fileEnPassant();
    assert (MY.enPassantPawns() <= ( MY.pawns() & MY.attackersTo(Square{file, Rank6}) ));
    for (Pi pi : MY.enPassantPawns()) {
        moves_.add(pi, Square{file, Rank5});
    }
}

template <Side::_t My>
void PositionMoves::populateUnderpromotions() {
    for (Pi pi : MY.promotables()) {
        // add underpromotions for each already generated legal queen promotion
        //TRICK: promoted piece type encoded inside pawn destination square rank
        Bb bb{moves_.bb(pi)};
        bb += bb.pBackward(); // Rook, Rank7
        bb |= bb.pBackward(); // Bishop, Rank6
        bb |= bb.pBackward(); // Knight, Rank5
        moves_.set(pi, bb);
    }
}

template <Side::_t My>
void PositionMoves::generateLegalKingMoves() {
    //TRICK: our attacks do not hide under attacked king shadow
    Bb kingMoves = ::attacksFrom(King, MY.sqKing()) % (MY.bbSide() | bbAttacked());
    moves_.set(Pi{TheKing}, kingMoves);
}

template <Side::_t My>
void PositionMoves::generateCastlingMoves() {
    for (Pi pi : MY.castlingRooks()) {
        if ( ::castlingRules.isLegal(MY.sqKing(), MY.sq(pi), OCCUPIED, bbAttacked()) ) {
            // castling encoded as the rook moves over the own king square
            moves_.add(pi, MY.sqKing());
        }
    }
}

template <Side::_t My>
void PositionMoves::generatePawnMoves() {
    for (Pi pi : MY.pawns()) {
        Square from{ MY.sq(pi) };

        Bb bb{ Bb{from}.pForward() % OCCUPIED }; // push
        bb += (bb & Bb{Rank3}).pForward() % OCCUPIED; // double push
        bb += ::attacksFrom(Pawn, from) & ~OP.bbSide(); // captures
        moves_.set(pi, bb);
    }
}

template <Side::_t My>
void PositionMoves::correctCheckEvasionsByPawns(Bb checkLine, Square checkFrom) {
    // simple pawn push over check line
    Bb potentialBlockers = checkLine.pBackward();

    // illegal phantom diagonal captures to fix
    Bb potentialInvalidCaptures = checkLine.pBackwardDiag();

    for (Square from : MY.bbPawns() & (potentialBlockers | potentialInvalidCaptures)) {
        Bb bb = (Bb{from.rankForward()} & checkLine) + (::attacksFrom(Pawn, from) & Bb{checkFrom});
        moves_.set(MY.pi(from), bb);
    }

    //TODO: refactor this
    // pawns double push over check line
    Bb pawnJumpEvasions = MY.bbPawns() & Bb{Rank2} & checkLine.pBackward().pBackward();
    pawnJumpEvasions %= OCCUPIED.pBackward(); // exlcude double push through occupied square
    for (Square from : pawnJumpEvasions) {
        moves_.add(MY.pi(from), Square{from.file(), Rank4});
    }
}

// exclude illegal moves due absolute pin
template <Side::_t My>
void PositionMoves::excludePinnedMoves(PiMask opPinners) {
    constexpr Side::_t Op{~My};

    for (Pi pinner : opPinners) {
        Square pinFrom{~OP.sq(pinner)};

        assert (::attacksFrom(OP.typeOf(pinner), pinFrom).has(MY.sqKing()));

        Bb pinLine = ::inBetween(MY.sqKing(), pinFrom);
        Bb occupiedPinLine = pinLine & OCCUPIED;
        assert (occupiedPinLine.any());

        if (occupiedPinLine.isSingleton() && occupiedPinLine.any(MY.bbSide())) {
            // we discovered a true pinned piece
            Pi pinned = MY.pi(occupiedPinLine.index());

            // exclude all pinned piece moves except those over the pin line
            moves_.filter(pinned, pinLine + Bb{pinFrom});
        }
    }
}

template <Side::_t My>
void PositionMoves::generateCheckEvasions() {
    constexpr Side::_t Op{~My};

    PiMask checkers = OP.checkers();

    if (!checkers.isSingleton()) {
        moves_ = {}; // double check case: no moves except king's ones are possible
    } else { // common single checker case
        Pi checker = checkers.pi();
        Square checkFrom{~OP.sq(checker)};
        Bb checkLine = ::inBetween(MY.sqKing(), checkFrom);

        // check evasion moves of all pieces
        // (including invalid phantom pawn captures and missing pawn non-captures)
        moves_.setAttacks(MY.attacks());
        moves_ &= checkLine + Bb{checkFrom};

        // pawns moves are special case
        correctCheckEvasionsByPawns<My>(checkLine, checkFrom);

        excludePinnedMoves<My>(OP.pinners() % checkers);

        populateUnderpromotions<My>();

        // trust out legal enpassant flag
        if (MY.hasEnPassant()) { assert (OP.enPassantPawns() == checkers); generateEnPassantMoves<My>(); }
    }

    generateLegalKingMoves<My>();
}

// generate all legal moves from the current position for the current side to move
template <Side::_t My>
void PositionMoves::generateMoves() {
    constexpr Side::_t Op{~My};
    bbAttacked_ = ~OP.attacks().bb();

    inCheck_ = bbAttacked().has(MY.sqKing());
    assert (OP.checkers().any() == bbAttacked().has(MY.sqKing()));

    if (inCheck_) {
        generateCheckEvasions<My>();
        return;
    }

    // create pseudolegal moves from piece attacks
    // (including invalid phantom pawn captures and missing pawn non-captures)
    moves_.setAttacks(MY.attacks());
    moves_ %= MY.bbSide();

    // pawns moves treated separately
    generatePawnMoves<My>();

    //TRICK: castling encoded as a rook move, so we implicitly cover the case of pinned castling in Chess960
    generateCastlingMoves<My>();

    excludePinnedMoves<My>(OP.pinners());

    // encoding underpromotions: generate pseudomoves mirroring already generated queen promotions
    populateUnderpromotions<My>();

    // trust out legal enpassant flag
    if (MY.hasEnPassant()) { generateEnPassantMoves<My>(); }

    generateLegalKingMoves<My>();
}

void PositionMoves::generateMoves() {
    generateMoves<My>();
    movesTotal_ = moves().popcount();
    movesMade_ = 0;
}
