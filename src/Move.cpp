#include "Move.hpp"
#include "Position.hpp"

Move::Move(const Position& pos, Square from, Square to) : v{from, to, Simple} {
    auto my = pos[My];

    if (my.kingSquare().is(to)) {
        v.special = Special;
        return;
    }

    if (my.isPawn(my.pieceOn(from))) {
        if (from.on(Rank7)) {
            v.special = Special;
            return;
        }
        if (from.on(Rank5) && to.on(Rank5)) {
            v.special = Special;
            return;
        }
    }
}

//convert internal move to long algebraic format
io::ostream& Move::write(io::ostream& out, Move move, Color colorToMove, ChessVariant chessVariant) {
    if (!move) { return out << "0000"; }

    Square moveFrom = move.from();
    Square moveTo = move.to();

    Square from = colorToMove.is(White) ? moveFrom : ~moveFrom;
    Square to = colorToMove.is(White) ? moveTo : ~moveTo;

    if (!move.isSpecial()) { return out << from << to; }

    //pawn promotion
    if (moveFrom.on(Rank7)) {
        //the type of a promoted pawn piece encoded in place of move to's rank
        Square promotedTo{File{moveTo}, colorToMove.is(White) ? Rank8 : Rank1};
        return out << from << promotedTo << ::promoTypeFrom(Rank{moveTo});
    }

    //en passant capture
    if (moveFrom.on(Rank5)) {
        //en passant capture move internally encoded as pawn captures pawn
        assert (moveTo.on(Rank5));
        return out << from << Square{File(moveTo), colorToMove.is(White) ? Rank6 : Rank3};
    }

    //castling
    if (moveFrom.on(Rank1)) {
        //castling move internally encoded as the rook captures the king
        switch (chessVariant) {
            case Chess960:
                return out << to << from;

            case Orthodox:
                if (from.on(FileA)) {
                    return out << to << Square{FileC, Rank(from)};
                }
                if (from.on(FileH)) {
                    return out << to << Square{FileG, Rank(from)};
                }
        }
    }

    //should never happen
    return out << "0000";
}

io::ostream& Move::write(io::ostream& out, const Move pv[], Color colorToMove, ChessVariant chessVariant) {
    for (Move move; (move = *pv); ++pv) {
        out << ' ';
        Move::write(out, move, colorToMove, chessVariant);
        colorToMove.flip();
    }
    return out;
}
