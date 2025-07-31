#include "Search.hpp"
#include "out.hpp"
#include "AttacksFrom.hpp"
#include "SearchRoot.hpp"
#include "PositionFen.hpp"
#include "UciGoLimit.hpp"

void SearchThread::run() {
    NodeAb{root.position, root}.visitRoot(limit.depth);
}

ReturnStatus NodeAb::visitRoot(Ply depthLimit) {
    auto rootMovesClone = moves;

    for (draft = 1; draft <= depthLimit; ++draft) {
        moves = rootMovesClone;
        score = NoScore;
        alpha = MinusInfinity;
        beta = PlusInfinity;
        BREAK_IF_ABORT ( searchMoves() );
        root.infoIterationEnd(draft);
        root.newIteration();
    }

    root.bestmove();
    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::visit(Move move) {
    alpha = -parent->beta;
    beta = -parent->alpha;
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    // mate-distance pruning
    alpha = std::max(alpha, Score::checkmated(ply));
    if (alpha >= beta) { return ReturnStatus::BetaCutoff; }

    score = NoScore;
    draft = parent->draft > 0 ? parent->draft-1 : 0;

    RETURN_IF_ABORT (root.countNode());
    parent->currentMove = parent->uciMove(move);
    makeMove(parent, move);
    ++parent->movesMade;
    repMask.update(rule50, parent->zobrist);

    bool inCheck = NodeAb::inCheck();

    if (moves.popcount() == 0) {
        //checkmated or stalemated
        score = inCheck ? Score::checkmated(ply) : Score{DrawScore};
    }
    else if (rule50 >= 100 || isRepetition()) {
        score = DrawScore;
    }
    else if (!inCheck && isDrawMaterial()) {
        score = DrawScore;
    }
    else if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
    }
    else if (!inCheck && draft == 0) {
        RETURN_IF_ABORT (quiescence());
    }
    else {
        RETURN_IF_ABORT (searchMoves());
    }

    return parent->negamax(-score);
}

ReturnStatus NodeAb::negamax(Score lastScore) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (lastScore > score) {
        score = lastScore;

        if (score >= beta) {
            //beta cut off
            updateKillerMove();
            return ReturnStatus::BetaCutoff;
        }

        if (score > alpha) {
            alpha = score;

            root.pvMoves.set(ply, currentMove);
            if (ply == 0) {
                root.infoNewPv(draft, score);
            }
        }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::searchMoves() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    NodeAb node{this};
    const auto child = &node;

    canBeKiller = false;

    RETURN_CUTOFF (child->visitIfLegal(root.pvMoves[ply]));

    RETURN_CUTOFF (goodCaptures(child));
    RETURN_CUTOFF (safePromotions(child));
    RETURN_CUTOFF (notBadCaptures(child));
    RETURN_CUTOFF (allPromotions(child));
    RETURN_CUTOFF (allCaptures(child));
    //TODO: underpromotions

    canBeKiller = true;

    if (parent) {
        RETURN_CUTOFF (child->visitIfLegal(parent->killer1));
        RETURN_CUTOFF (child->visitIfLegal(parent->killer2));

        if (parent->grandParent) {
            RETURN_CUTOFF (child->visitIfLegal(parent->grandParent->killer1));
        }

        Move move = parent->currentMove;
        PieceType ty = (*parent)[My].typeOf(move.from());
        RETURN_CUTOFF (child->visitIfLegal( root.counterMove(colorToMove(), ty, move.to()) ));
    }

    // safe non capture moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi] % attackedSquares) {
            RETURN_CUTOFF (child->visit({from, to}));
        }
    }

    // all the rest moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_CUTOFF (child->visit({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    //stand pat
    score = evaluate();
    if (beta <= score) {
        return ReturnStatus::BetaCutoff;
    }
    if (alpha < score) {
        alpha = score;
    }

    NodeAb node{this};
    const auto child = &node;

    RETURN_CUTOFF (goodCaptures(child));
    RETURN_CUTOFF (safePromotions(child));
    RETURN_CUTOFF (notBadCaptures(child, lastMovedTo));

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::goodCaptures(NodeAb* child) {
    // MVV (most valuable victim) order
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (goodCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::notBadCaptures(NodeAb* child) {
    // MVV (most valuable victim) order
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (notBadCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::allCaptures(NodeAb* child) {
    // MVV (most valuable victim)
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (allCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::goodCaptures(NodeAb* child, Square to) {
    PiMask attackers = moves[to];

    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }
    if (attackers.none()) { return ReturnStatus::Continue; }

    bool isVictimProtected = attackedSquares.has(to);
    assert (isVictimProtected == OP.attackersTo(~to).any());
    if (isVictimProtected) {
        // filter out more valuable attackers than the victim
        attackers &= MY.goodKillers( OP.typeOf(OP.pieceAt(~to)) );
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.least(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->visit(Move{from, to}));
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::notBadCaptures(NodeAb* child, Square to) {
    PiMask attackers = moves[to];

    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }
    if (attackers.none()) { return ReturnStatus::Continue; }

    bool isVictimProtected = attackedSquares.has(to);
    assert (isVictimProtected == OP.attackersTo(~to).any());
    if (isVictimProtected) {
        // filter out more valuable attackers than the victim
        attackers &= MY.notBadKillers( OP.typeOf(OP.pieceAt(~to)) );
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.least(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->visit(Move{from, to}));
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::allCaptures(NodeAb* child, Square to) {
    PiMask attackers = moves[to];
    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.least(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->visit(Move{from, to}));
    }

    return ReturnStatus::Continue;
}

// non capture queen promotions to the unattacked squares
ReturnStatus NodeAb::safePromotions(NodeAb* child) {
    for (Pi pi : MY.promotables()) {
        // skip moves to the attacked square
        for (Square to : moves[pi] % attackedSquares & Bb{Rank8}) {
            Square from = MY.squareOf(pi);
            RETURN_CUTOFF( child->visit({from, to}) );
        }
    }

    return ReturnStatus::Continue;
}

// all non capture queen promotions
ReturnStatus NodeAb::allPromotions(NodeAb* child) {
    for (Pi pi : MY.promotables()) {
        for (Square to : moves[pi] & Bb{Rank8}) {
            Square from = MY.squareOf(pi);
            RETURN_CUTOFF( child->visit({from, to}) );
        }
    }

    return ReturnStatus::Continue;
}

void NodeAb::updateKillerMove() {
    if (!canBeKiller) { return; }
    if (!parent) { return; }

    if (parent->killer1 != currentMove) {
        parent->killer2 = parent->killer1;
        parent->killer1 = currentMove;
    }

    Move move = parent->currentMove;
    PieceType ty = (*parent)[My].typeOf(move.from());
    root.counterMove.set(colorToMove(), ty, move.to(), currentMove);
}

UciMove NodeAb::uciMove(Square from, Square to) const {
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.position.getChessVariant()};
}

Color NodeAb::colorToMove() const {
    return root.position.getColorToMove(ply);
}

Score NodeAb::evaluate()
{
    return Position::evaluate().clamp();
}

bool NodeAb::isDrawMaterial() const {
    if (rule50 == 0) {
        // insufficient material
        auto& my = root.position[My].evaluation;
        auto& op = root.position[Op].evaluation;
        if (my.count(Pawn) == 0 && op.count(Pawn) == 0) {
            if (my.material() <= 4 && op.material() <= 4) { return true; }
            if (my.material() <= 4 && op.count(Knight) == 2) { return true; }
            if (op.material() <= 4 && my.count(Knight) == 2) { return true; }
            auto myMinors = my.count(Knight) + my.count(Bishop);
            auto opMinors = op.count(Knight) + op.count(Bishop);
            if (myMinors == 2 && opMinors == 1 && !(my.count(Bishop) == 2)) { return true; }
            if (opMinors == 2 && myMinors == 1 && !(op.count(Bishop) == 2)) { return true; }
        }
    }
    return false;
}

bool NodeAb::isRepetition() const {
    if (rule50 < 4) { return false; }

    const Z& z = zobrist;

    if (grandParent) {
        auto next = grandParent;
        while ((next = next->grandParent)) {
            if (next->zobrist == z) {
                return true;
            }
            if (!next->repMask.has(z)) { break; }
        }
    }

    return root.repetition.has(colorToMove(), z);
}
