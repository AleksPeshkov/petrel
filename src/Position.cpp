#include "Position_impl.hpp"

Score Position::evaluate() const {
    auto eval = accumulator.evaluate();
    return Score::clampEval(eval);
}

void Position::flip(const Position& parent) {
    // copy from the parent position but swap sides
    accumulator.flip(parent.accumulator);
    positionSide_[My] = parent._(Op);
    positionSide_[Op] = parent._(My);
    rule50_ = parent.rule50_;
}

void Position::makeMove(Square from, Square to) {
    PositionSide::swap(_(My), _(Op));
    accumulator.swap();

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op, Full>(from, to, []{});
    zobrist_.flip();
    //assert (z() == *generateZobrist()); // true, but slow to compute
}

void Position::makeNullMove(const Position& parent) {
    flip(parent);
    zobrist_ = parent.zobrist_;
    rule50_.next(); zHash_ = {}; // null move holds rule50, but not ZHash

    occupied_[My] = parent.occupied_[Op];
    occupied_[Op] = parent.occupied_[My];

    // clear en passant status from the previous move
    if (_(My).hasEnPassant()) {
        zobrist_.opEnPassant(_(My).sqEnPassant());
        _(Op).clearEnPassantKillers();
        _(My).clearEnPassantVictim();
    }

    zobrist_.flip();
    //assert (z() == *generateZobrist()); // true, but slow to compute
}

bool Position::setEnPassant(File file) {
    Square ep{file, Rank4}; // not FEN ep square, but victim pawn location

    if (!_(Op).isPawn(ep) || bbBoth(My).has({file, Rank6})) {
        // pseudo legal test failed
        return false;
    }

    // full legality check
    setLegalEnPassant<Op>(ep);
    return true;
}

bool Position::dropValid(Side side, Piece piece, Square to) {
    return _(side).dropValid(piece, to);
}

bool Position::afterDrop() {
    PositionSide::finalSetup(_(My), _(Op));
    updateSliderAttacks<Op>(_(Op).any(), _(My).any());
    accumulator.setup(*this);
    rule50_ = {};

    // opponent should not be in check
    return _(My).checkers().isNone();
}

Bb Position::bbPassedPawns() const {
    Bb bbBlockers = ~(_(Op).bbPawns() | _(Op).bbPawnAttacks().forward());
    for (int i = 0; i < 5; ++i) {
        bbBlockers |= bbBlockers.backward();
    }
    return _(My).bbPawns() % bbBlockers;
}

template <Side::_t My>
Zobrist Position::generateZobrist() const {
    Zobrist z{};

    for (Pi pi : _(My).any()) { z(_(My).piece(pi), _(My).sq(pi));}
    for (Pi piRook : _(My).castlingRooks()) { z.castling(_(My).sq(piRook)); }

    return z;
}

Zobrist Position::generateZobrist() const {
    constexpr Side::_t Op{~My};

    Zobrist z{generateZobrist<My>(), generateZobrist<Op>()};
    if (_(Op).hasEnPassant()) { z.opEnPassant(_(Op).sqEnPassant()); }

    return z;
}
