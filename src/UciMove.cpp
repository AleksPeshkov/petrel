#include "UciMove.hpp"

// convert move to UCI format
ostream& operator << (ostream& out, const UciMove& move) {
    if (!move) { return out << "0000"; }

    auto isWhite{ move.color == White };
    Square from_ = move.from();
    Square to_ = move.to();
    Square from = isWhite ? from_ : ~from_;
    Square to = isWhite ? to_ : ~to_;

    if (move.type == UciMove::Normal) { return out << from << to; }

    //pawn promotion
    if (from_.on(Rank7)) {
        //the type of a promoted pawn piece encoded in place of move to's rank
        Square promotedTo{File{to}, isWhite ? Rank8 : Rank1};
        return out << from << promotedTo << PromoType{::promoTypeFrom(Rank{to_})};
    }

    //en passant capture
    if (from_.on(Rank5)) {
        //en passant capture move internally encoded as pawn captures pawn
        assert (to_.on(Rank5));
        return out << from << Square{File{to}, isWhite ? Rank6 : Rank3};
    }

    //castling
    if (from_.on(Rank1)) {
        //castling move internally encoded as the rook captures the king

        if (move.variant == Orthodox) {
            if (from.on(FileA)) { return out << to << Square{FileC, Rank{from}}; }
            if (from.on(FileH)) { return out << to << Square{FileG, Rank{from}}; }
        }

        // Chess960:
        return out << to << from;
    }

    //should never happen
    assert (false);
    return out << "0000";
}

ostream& operator << (ostream& out, const UciMove pv[]) {
    for (UciMove move; (move = *pv++); ) {
        out << " " << move;
    }
    return out;
}
