#include "PositionMoves.hpp"

#include "CastlingRules.hpp"
#include "AttacksFrom.hpp"

template <Side::_t My>
void PositionMoves::generateEnPassantMoves() {
    constexpr Side Op{~My};

    assert (MY.hasEnPassant());
    assert (OP.hasEnPassant());

    File epFile = OP.enPassantFile();
    assert (MY.enPassantPawns() <= ( MY.pawns() & MY.attackersTo(Square{epFile, Rank6}) ));

    moves[Rank5] |= PiRank(epFile) & PiRank{MY.enPassantPawns()};
}

template <Side::_t My>
void PositionMoves::populateUnderpromotions() {
    constexpr Side Op{~My};

    //add underpromotions for each already generated legal queen promotion
    //TRICK: promoted piece type encoded inside pawn destination square rank
    PiRank promotionFiles = moves[Rank8] & PiRank{MY.pawns()};
    moves[::rankOf(Rook)]   += promotionFiles;
    moves[::rankOf(Bishop)] += promotionFiles;
    moves[::rankOf(Knight)] += promotionFiles;
}

template <Side::_t My>
void PositionMoves::generateLegalKingMoves() {
    constexpr Side Op{~My};

    //TRICK: our attacks do not hide under attacked king shadow
    Bb kingMoves = ::attacksFrom(King, MY.kingSquare()) % (MY.piecesSquares() | attackedSquares);
    moves.set(TheKing, kingMoves);
}

template <Side::_t My>
void PositionMoves::generateCastlingMoves() {
    constexpr Side Op{~My};

    for (Pi pi : MY.castlingRooks()) {
        if ( ::castlingRules.isLegal(MY.kingSquare(), MY.squareOf(pi), occupied<My>(), attackedSquares) ) {
            //castling encoded as the rook moves over the own king square
            moves.add(pi, MY.kingSquare());
        }
    }
}

template <Side::_t My>
void PositionMoves::generatePawnMoves() {
    constexpr Side Op{~My};

    for (Pi pi : MY.pawns()) {
        Square from{ MY.squareOf(pi) };

        Rank rankTo{ ::rankForward(Rank(from)) };
        BitRank fileTo{ File{from} };

        //push to free square
        fileTo %= occupied<My>()[rankTo];

        //double push
        if ((rankTo.is(Rank3)) && (fileTo % occupied<My>()[Rank4]).any()) {
            moves.set(pi, Rank4, fileTo);
        }

        //remove "captures" of free squares from default generated moves
        fileTo += moves[rankTo][pi] & occupied<My>()[rankTo];

        moves.set(pi, rankTo, fileTo);
    }
}

template <Side::_t My>
void PositionMoves::correctCheckEvasionsByPawns(Bb checkLine, Square checkFrom) {
    constexpr Side Op{~My};

    //TRICK: assumes Rank8 = 0
    //simple pawn push over check line
    Bb potencialEvasions = checkLine << 8u;

    //the general case generates invalid diagonal moves to empty squares
    Bb invalidDiagonalMoves = (checkLine << 9u) % Bb{FileA} | (checkLine << 7u) % Bb{FileH};

    Bb affectedPawns = MY.pawnsSquares() & (potencialEvasions | invalidDiagonalMoves);
    for (Square from : affectedPawns) {
        Bb bb = (Bb{from.rankForward()} & checkLine) + (::attacksFrom(Pawn, from) & checkFrom);
        Rank rankTo = ::rankForward(Rank(from));
        moves.set(MY.pieceAt(from), rankTo, bb[rankTo]);
    }

    //pawns double push over check line
    Bb pawnJumpEvasions = MY.pawnsSquares() & Bb{Rank2} & (checkLine << 16u) % (occupied<My>() << 8u);
    for (Square from : pawnJumpEvasions) {
        moves.add(MY.pieceAt(from), Rank4, File(from));
    }

}

//exclude illegal moves due absolute pin
template <Side::_t My>
void PositionMoves::excludePinnedMoves(PiMask opPinners) {
    constexpr Side Op{~My};

    for (Pi pinner : opPinners) {
        Square pinFrom{~OP.squareOf(pinner)};

        assert (::attacksFrom(OP.typeOf(pinner), pinFrom).has(MY.kingSquare()));

        const Bb& pinLine = ::inBetween(MY.kingSquare(), pinFrom);
        Bb piecesOnPinLine = pinLine & occupied<My>();
        assert (piecesOnPinLine.any());

        if (piecesOnPinLine.isSingleton() && (piecesOnPinLine & MY.piecesSquares()).any()) {
            //we discovered a true pinned piece
            Pi pinned = MY.pieceAt(piecesOnPinLine.index());

            //exclude all pinned piece moves except those over the pin line
            moves.filter(pinned, pinLine + pinFrom);
        }
    }
}

template <Side::_t My>
void PositionMoves::generateCheckEvasions() {
    constexpr Side Op{~My};

    PiMask checkers = OP.checkers();

    if (checkers.isSingleton()) {
        //single checker case
        Pi checker = checkers.index();
        Square checkFrom{~OP.squareOf(checker)};

        const Bb& checkLine = ::inBetween(MY.kingSquare(), checkFrom);

        //general case: check evasion moves of all pieces
        moves = MY.attacksMatrix() & (checkLine + checkFrom);

        //pawns moves are special case
        correctCheckEvasionsByPawns<My>(checkLine, checkFrom);

        excludePinnedMoves<My>(OP.pinners() % checkers);

        populateUnderpromotions<My>();

        if (MY.hasEnPassant()) { assert (OP.enPassantPawns() == checkers); generateEnPassantMoves<My>(); }
    }
    else {
        //double check case: no moves except king's ones are possible
        moves.clear();
    }

    generateLegalKingMoves<My>();
}

//generate all legal moves from the current position for the current side to move
template <Side::_t My>
void PositionMoves::generateMoves() {
    constexpr Side Op{~My};
    attackedSquares = ~OP.attacksMatrix().gather();

    inCheck = attackedSquares.has(MY.kingSquare());
    assert (OP.checkers().any() == attackedSquares.has(MY.kingSquare()));

    if (inCheck) {
        generateCheckEvasions<My>();
        return;
    }

    //the most general case: captures and non captures for all pieces
    moves = MY.attacksMatrix() % MY.piecesSquares();

    //pawns moves treated separately
    generatePawnMoves<My>();

    generateCastlingMoves<My>();

    //TRICK: castling encoded as a rook move, so we implicitly cover the case of pinned castling in Chess960
    excludePinnedMoves<My>(OP.pinners());

    populateUnderpromotions<My>();

    if (MY.hasEnPassant()) { generateEnPassantMoves<My>(); }

    generateLegalKingMoves<My>();
}

void PositionMoves::makeMoves() {
    generateMoves<My>();
}

bool PositionMoves::isLegalMove(Square from, Square to) const {
    return MY.has(from) && moves.has(MY.pieceAt(from), to);
}

// make irreversible move in this position itself
void PositionMoves::makeMove(Square from, Square to) {
    Position::makeMove(from, to);
    makeMoves();
}

void PositionMoves::makeMove(PositionMoves* parent, Square from, Square to) {
    parent->moves.clear((*parent)[My].pieceAt(from), to);
    Position::makeMove(parent, from, to);
    makeMoves();
}

void PositionMoves::makeMoveNoZobrist(PositionMoves* parent, Square from, Square to) {
    parent->moves.clear((*parent)[My].pieceAt(from), to);
    Position::makeMoveNoZobrist(parent, from, to);
    makeMoves();
}
