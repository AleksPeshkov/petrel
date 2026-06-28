#include "Position_impl.hpp"

Score Position::evaluate() const {
    auto eval = accumulator.evaluate();
    return Score::clampEval(eval);
}

void Position::flip(const Position& parent) {
    // copy from the parent position but swap sides
    accumulator.flip(parent.accumulator);
    positionSide_[Side{My}] = parent.OP;
    positionSide_[Side{Op}] = parent.MY;
    rule50_ = parent.rule50_;
}

void Position::makeMove(Square from, Square to) {
    PositionSide::swap(MY, OP);
    accumulator.flip();

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op, Full>(from, to, []{});
    zobrist_.flip();
    //assert (z() == *generateZobrist()); // true, but slow to compute
}

void Position::makeMoveNoEval(Square from, Square to) {
    PositionSide::swap(MY, OP);
    //skip accumulator.swap();

    // the position just swapped its sides, so we make the move for the Op
    makeMove<Op, NoEval>(from, to, []{});
    zobrist_.flip();
    //assert (z() == *generateZobrist()); // true, but slow to compute
}

void Position::makeNullMove(const Position& parent) {
    flip(parent);
    zobrist_ = parent.zobrist_;
    // null move
    rule50_.next();
    occupied_[Side{My}] = parent.occupied_[Side{Op}];
    occupied_[Side{Op}] = parent.occupied_[Side{My}];

    // clear en passant status from the previous move
    if (MY.hasEnPassant()) {
        zobrist_.opEnPassant(MY.sqEnPassant());
        OP.clearEnPassantKillers();
        MY.clearEnPassantVictim();
    }

    zobrist_.flip();
    //assert (z() == *generateZobrist()); // true, but slow to compute
}

void Position::makeMoveFast(const Position& parent, Square from, Square to) {
    flip(parent);

    // current position flipped its sides relative to parent, so we make the move inplace for the Op
    makeMove<Op, Fast>(from, to, []{});

    //assert (z() == Z{} || z() == *generateZobrist()); // true, but slow to compute
}

bool Position::setEnPassant(File file) {
    Square ep{file, Rank4}; // not FEN ep square, but victim pawn location

    if (!OP.isPawn(ep) || OCCUPIED.has(Square{file, Rank6})) {
        // pseudo legal test failed
        return false;
    }

    // full legality check
    setLegalEnPassant<Op>(ep);
    return true;
}

bool Position::dropValid(Side si, PieceType ty, Square to) {
    accumulator.drop(si, ty, to);
    return positionSide(si).dropValid(ty, to);
}

bool Position::afterDrop() {
    PositionSide::finalSetup(MY, OP);
    updateSliderAttacks<Op>(OP.any(), MY.any());
    rule50_ = {};

    // opponent should not be in check
    return MY.checkers().none();
}

Bb Position::bbPassedPawns() const {
    Bb blockers = ~(OP.bbPawns() | OP.bbPawnAttacks().pForward());
    for (int i = 0; i < 5; ++i) {
        blockers |= blockers.pBackward();
    }
    return MY.bbPawns() % blockers;
}

template <Side::_t My>
Zobrist Position::generateZobrist() const {
    Zobrist z{};

    for (Pi pi : MY.any()) { z(MY.typeOf(pi), MY.sq(pi));}
    for (Pi rook : MY.castlingRooks()) { z.castling(MY.sq(rook)); }

    return z;
}

Zobrist Position::generateZobrist() const {
    constexpr Side::_t Op{~My};

    Zobrist z{generateZobrist<My>(), generateZobrist<Op>()};
    if (OP.hasEnPassant()) { z.opEnPassant(OP.sqEnPassant()); }

    return z;
}
