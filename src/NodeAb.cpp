#include "NodeAb.hpp"
#include "out.hpp"
#include "AttacksFrom.hpp"
#include "CastlingRules.hpp"
#include "SearchControl.hpp"
#include "PositionFen.hpp"

NodeControl NodeAb::visit(Move move) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    RETURN_IF_ABORT (control.countNode());

    score = NoScore;
    alpha = -parent.beta;
    beta = -parent.alpha;

    draft = parent.draft > 0 ? parent.draft-1 : 0;

    parent.currentMove = parent.createFullMove(move);
    ++parent.movesVisited;
    makeMove(parent, move);

    bool inCheck = NodeAb::inCheck();

    if (movesCount() == 0) {
        //checkmated or stalemated
        return parent.negamax(inCheck ? Score::checkmated(ply) : DrawScore);
    }

    if (ply == MaxPly) {
        // no room to search deeper
        return parent.negamax(staticEval());
    }

    if (draft == 0 && !inCheck) {
        RETURN_IF_ABORT (quiescence());
    }
    else {
        RETURN_IF_ABORT (visitChildren());
    }

    return parent.negamax(score);
}

NodeControl NodeAb::negamax(Score childScore) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (score < -childScore) {
        score = -childScore;

        if (beta <= score) {
            //beta cut off
            setKiller();
            return NodeControl::BetaCutoff;
        }

        if (alpha < score) {
            alpha = score;

            control.pvMoves.set(ply, currentMove);
            if (ply == 0) {
                control.infoNewPv(draft, score);
            }
        }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    return NodeControl::Continue;
}

NodeControl NodeAb::visitChildren() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    NodeAb child{*this};

    canBeKiller = false;

    CUTOFF (child.visitIfLegal(control.pvMoves[ply]));

    CUTOFF (goodCaptures(child));

    canBeKiller = true;

    if (ply >= 1) {
        CUTOFF (child.visitIfLegal(parent.killer1));
        CUTOFF (child.visitIfLegal(parent.killer2));
    }
    if (ply >= 3) {
        CUTOFF (child.visitIfLegal(parent.parent.parent.killer1));
    }

    // the rest of the remaining unsorted moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            CUTOFF (child.visit({from, to}));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    //stand pat
    score = staticEval();
    if (beta <= score) {
        return NodeControl::BetaCutoff;
    }
    if (alpha < score) {
        alpha = score;
    }

    NodeAb child{*this};

    //CUTOFF (recapture(child));

    CUTOFF (goodCaptures(child));

    return NodeControl::Continue;
}

NodeControl NodeAb::recapture(NodeAb& child) {
    if (ply == 0) { return NodeControl::Continue; }

    Move move = parent.currentMove;
    Square to = move.to();

    assert (move.isExternal());
    if (move.isSpecial()) {
        Square from = move.from();
        Pi victim = parent[My].pieceAt(from);

        if (move.from().on(Rank7)) {
            // pawn promotion
            assert(parent[My].isPromotable(victim));
            to = {File{to}, Rank8};
        }
        else if (to.on(Rank5)) {
            // en passant
            assert (from.on(Rank5));
            to = {File{to}, Rank6};
            assert (parent[My].isPawn(victim));
            assert (parent[My].enPassantPawns().has(victim));
        }
        else {
            // castling
            assert (from.on(Rank1));
            assert (parent[My].squareOf(TheKing) == to);
            to = CastlingRules::castlingRookTo(to, from);
        }
    }

    return goodCaptures(child, ~to);
}

NodeControl NodeAb::goodCaptures(NodeAb& child) {
    // MVV (most valuable victim)
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        CUTOFF (goodCaptures(child, to));
    }

    // non capture promotions
    for (Pi pi : MY.promotables()) {
        for (Square to : moves[pi] & Bb{Rank8}) {
            // skip move to the defended square
            if (isOpAttacks(to)) { continue; }

            Square from = MY.squareOf(pi);
            // to the queen
            CUTOFF( child.visit({from, to}) );

            // underpromotion to the knight only if it makes check
            if (attacksFrom(Knight, ~OP.squareOf(TheKing)).has(to)) {
                Square toKnight{ File{to}, ::rankOf(Knight) };
                CUTOFF( child.visit({from, toKnight}) );
            }
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::goodCaptures(NodeAb& child, Square to) {
    assert (OP.piecesSquares().has(~to));

    PiMask attackers = moves[to];
    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }
    if (attackers.none()) { return NodeControl::Continue; }

    bool isVictimProtected = isOpAttacks(to);
    assert (isVictimProtected == OP.attackersTo(~to).any());

    if (isVictimProtected) {
        // filter out more valuable attackers than the victim
        attackers &= MY.goodKillers( OP.typeOf(OP.pieceAt(~to)) );
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.least(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        CUTOFF (child.visit(Move{from, to}));
    }

    return NodeControl::Continue;
}

void NodeAb::setKiller() {
    if (
        ply >= 1
        && canBeKiller
        && parent.killer1 != currentMove
    ) {
        parent.killer2 = parent.killer1;
        parent.killer1 = currentMove;
    }
}

Move NodeAb::createFullMove(Square from, Square to) const {
    Color color = control.position.getColorToMove() << ply;
    return Move{from, to, isSpecial(from, to), color, control.position.getChessVariant()};
}

Score NodeAb::staticEval()
{
    return Position::evaluate().clamp();
}
