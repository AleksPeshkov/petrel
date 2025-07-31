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
    repMask = root.repetition.repMask(colorToMove());

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

    draft = parent->draft > 0 ? parent->draft-1 : 0;

    RETURN_IF_ABORT (root.countNode());
    parent->childMove = move;
    makeMove(parent, move);
    root.pvMoves.set(ply, UciMove{});
    ++parent->movesMade;

    if (rule50 <= 1) { repMask = RepetitionMask{}; }
    else if (grandParent) { repMask = RepetitionMask{grandParent->repMask, grandParent->zobrist}; }
    else { repMask = root.repetition.repMask(colorToMove()); }

    canBeKiller = false;


    if (moves.popcount() == 0) {
        //checkmated or stalemated
        score = inCheck ? Score::checkmated(ply) : Score{DrawScore};
    }
    else if (rule50 >= 100 || isRepetition() || isDrawMaterial()) {
        score = DrawScore;
    }
    else if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
    }
    else if (draft == 0 && !inCheck) {
        RETURN_IF_ABORT (quiescence());
    }
    else {
        score = NoScore;
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

            root.pvMoves.set(ply, uciMove(childMove));
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

    RETURN_CUTOFF (child->visitIfLegal(root.pvMoves[ply]));

    RETURN_CUTOFF (goodCaptures(child));
    RETURN_CUTOFF (safePromotions(child));
    RETURN_CUTOFF (notBadCaptures(child));
    RETURN_CUTOFF (allPromotions(child));
    //TODO: underpromotions

    canBeKiller = true;

    Pi lastPi = TheKing;
    Bb newMoves = {};

    //TODO: checking moves

    if (parent) {
        // killer move to be tried first
        RETURN_CUTOFF (child->visitIfLegal(parent->killer1));

        // counter moves may refute the last opponent move
        Move move = parent->childMove;
        PieceType ty = (*parent)[My].typeOf(move.from());
        RETURN_CUTOFF (child->visitIfLegal( root.counterMove(colorToMove(), ty, move.to()) ));

        RETURN_CUTOFF (child->visitIfLegal(parent->killer2));

        // try quiet moves of the last moved piece (unless it was captured)
        {
            Square from = parent->lastPieceTo;
            if (MY.piecesSquares().has(from)) {
                // last moved piece
                lastPi = MY.pieceAt(from);

                // new moves of the last moved piece
                newMoves = moves[lastPi];

                if (from != parent->lastPieceFrom) {
                    // unless it was a pawn promotion move
                    newMoves %= (*parent)[My].attacksOf(lastPi);
                }

                // try new safe moves of the last moved piece
                for (Square to : newMoves % attackedSquares) {
                    RETURN_CUTOFF (child->visit({from, to}));
                }

                // keep unsafe news moves for later
                newMoves &= attackedSquares;
            }
        }

        // new safe quiet moves, except for the last moved piece (or king)
        for (Pi pi : MY.pieces() - lastPi) {
            Square from = MY.squareOf(pi);
            for (Square to : moves[pi] % (*parent)[My].attacksOf(pi) % attackedSquares) {
                RETURN_CUTOFF (child->visit({from, to}));
            }
        }
    }

    // all the rest safe quiet moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : moves[pi] % attackedSquares) {
            RETURN_CUTOFF (child->visit({from, to}));
        }
    }

    if (newMoves.any()) {
        Square from = parent->lastPieceTo;
        Pi pi = MY.pieceAt(from);

        // unsafe new moves of the last moved piece
        for (Square to : newMoves) {
            RETURN_CUTOFF (child->visitIfLegal({from, to}));
        }

        // the rest moves of the last moved piece
        for (Square to : moves[pi]) {
            RETURN_CUTOFF (child->visit({from, to}));
        }
    }

    // unsafe (bad) captures
    RETURN_CUTOFF (allCaptures(child));

    // all the rest moves, LV first
    auto pieces = MY.pieces();
    while (pieces.any()) {
        Pi pi = pieces.leastValuable(); pieces -= pi;
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_CUTOFF (child->visit({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodeAb::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck);

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
    RETURN_CUTOFF (notBadCaptures(child));
    RETURN_CUTOFF (allPromotions(child));

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
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

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
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

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
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

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

    if (parent->killer1 != childMove) {
        parent->killer2 = parent->killer1;
        parent->killer1 = childMove;
    }

    Move move = parent->childMove;
    PieceType ty = (*parent)[My].typeOf(move.from());
    root.counterMove.set(colorToMove(), ty, move.to(), childMove);
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

// insufficient mate material
bool NodeAb::isDrawMaterial() const {
    auto& my = MY.evaluation;
    auto& op = OP.evaluation;

    if (my.hasMatingPieces() || op.hasMatingPieces()) { return false; }

    // here both sides can have only minors pieces
    auto myMinors = my.count(Knight) + my.count(Bishop);
    auto opMinors = op.count(Knight) + op.count(Bishop);

    // lone minors cannot mate
    if (myMinors <= 1 && opMinors <= 1) { return true; }

    if (myMinors == 2) {
        if (my.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (my.count(Bishop) == 1 && opMinors == 1) { return true; }
        if (my.count(Bishop) == 2 && op.count(Bishop) == 1) { return true; }
    }

    if (opMinors == 2) {
        if (op.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (op.count(Bishop) == 1 && myMinors == 1) { return true; }
        if (op.count(Bishop) == 2 && my.count(Bishop) == 1) { return true; }
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
            if (!next->repMask.has(z)) {
                return false;
            }
        }
    }

    return root.repetition.has(colorToMove(), z);
}
