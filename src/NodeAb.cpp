#include "NodeAb.hpp"
#include "out.hpp"
#include "SearchControl.hpp"
#include "PositionFen.hpp"

NodeControl NodeAb::visit(Move move) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    RETURN_IF_ABORT (control.countNode());

    score = NoScore;
    alpha = -parent.beta;
    beta = -parent.alpha;

    draft = parent.draft > 0 ? parent.draft-1 : 0;
    makeMove(parent, move);

    bool inCheck = NodeAb::inCheck();

    if (movesCount() == 0) {
        //checkmated or stalemated
        score = inCheck ? Score::checkmated(ply) : DrawScore;
        return parent.negamax(score, move);
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = staticEval();
        return parent.negamax(score, move);
    }

    if (draft == 0 && !inCheck) {
        RETURN_IF_ABORT (quiescence(move));
    }
    else {
        RETURN_IF_ABORT (visitChildren());
    }

    return parent.negamax(score, move);
}

NodeControl NodeAb::negamax(Score childScore, Move move) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (score < -childScore) {
        score = -childScore;

        if (beta <= score) {
            //beta cut off
            return NodeControl::BetaCutoff;
        }

        if (alpha < score) {
            alpha = score;

            control.pvMoves.set(ply, createFullMove(move));
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

    const Move& move = control.pvMoves[ply];
    if (isLegalMove(move)) {
        CUTOFF (child.visit(move));
    }

    CUTOFF (goodCaptures(child));

    // the rest of the remaining unsorted moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            CUTOFF (child.visit({from, to}));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::goodCaptures(NodeAb& child) {
    //MVV
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        //LVA
        Square to = ~OP.squareOf(victim);
        PiMask attackers = moves[to];
        if (attackers.none()) { continue; }

        bool isVictimProtected = isOpAttacks(to);
        if (isVictimProtected) {
            assert (OP.attackersTo(~to).any());
            // filter out more valuable attackers than the victim
            attackers &= MY.goodKillers(OP.typeOf(victim));
            if (attackers.none()) { continue; }
        } else {
            assert (OP.attackersTo(~to).none());
        }

        while (attackers.any()) {
            Pi attacker = attackers.least(); attackers -= attacker;

            Square from = MY.squareOf(attacker);

            if (from.on(Rank7) && MY.isPawn(attacker) && !to.on(Rank8)) {
                //skip underpromotion pseudo moves
                //TODO: make it faster
                continue;
            }

            CUTOFF (child.visit(Move{from, to}));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::quiescence(Move) {
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
    CUTOFF (goodCaptures(child));

    return NodeControl::Continue;
}

Score NodeAb::staticEval()
{
    return Position::evaluate().clamp();
}

Move NodeAb::createFullMove(Square from, Square to) const {
    Color color = control.position.getColorToMove() << ply;
    return Move{from, to, isSpecialMove(from, to), color, control.position.getChessVariant()};
}
