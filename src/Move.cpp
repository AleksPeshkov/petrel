#include "Move.hpp"

// convert move to UCI format
io::ostream& operator << (io::ostream& out, Move move) {
    assert (!move || move.isExternal());
    if (!move || move.type == Move::Internal) { return out << "0000"; }

    auto isWhite{ move.color == White };
    Square _from{ move._from };
    Square _to{ move._to };
    Square from{ isWhite ? _from : ~_from };
    Square to{ isWhite ? _to : ~_to };

    if (move.type == Move::Normal) { return out << from << to; }

    //pawn promotion
    if (_from.on(Rank7)) {
        //the type of a promoted pawn piece encoded in place of move to's rank
        Square promotedTo{File{to}, isWhite ? Rank8 : Rank1};
        return out << from << promotedTo << PromoType{::promoTypeFrom(Rank{_to})};
    }

    //en passant capture
    if (_from.on(Rank5)) {
        //en passant capture move internally encoded as pawn captures pawn
        assert (_to.on(Rank5));
        return out << from << Square{File(to), isWhite ? Rank6 : Rank3};
    }

    //castling
    if (_from.on(Rank1)) {
        //castling move internally encoded as the rook captures the king
        if (move.variant == Chess960) { return out << to << from; }

        // Orthodox:
        if (from.on(FileA)) { return out << to << Square{FileC, Rank(from)}; }
        if (from.on(FileH)) { return out << to << Square{FileG, Rank(from)}; }
    }

    //should never happen
    assert (false);
    return out << "0000";
}

io::ostream& operator << (io::ostream& out, const Move pv[]) {
    for (Move move; (move = *pv); ++pv) {
        out << " " << move;
    }
    return out;
}
