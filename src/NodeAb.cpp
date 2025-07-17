#include "NodeAb.hpp"
#include "out.hpp"
#include "SearchControl.hpp"
#include "UciSearchInfo.hpp"
#include "PositionFen.hpp"

NodeControl NodeAb::visit(Move move) {
    RETURN_IF_ABORT (control.countNode());

    score = NoScore;
    alpha = -parent.beta;
    beta = -parent.alpha;

    draft = parent.draft > 0 ? parent.draft-1 : 0;
    makeMove(parent, move);

    bool inCheck = NodeAb::inCheck();

    if (movesCount() == 0) {
        //checkmated or stalemated
        score = inCheck ? Score::checkmated(ply) : Score{DrawScore};
        return parent.negamax(score, move);
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = staticEval();
        return parent.negamax(score, move);
    }

    if (draft == 0 && !inCheck) {
        score = staticEval();
    }
    else {
        RETURN_IF_ABORT (visitChildren());
    }

    return parent.negamax(score, move);
}

NodeControl NodeAb::negamax(Score childScore, Move move) {
    if (score < -childScore) {
        score = -childScore;

        if (beta <= score) {
            //beta cut off
            return NodeControl::BetaCutoff;
        }

        if (alpha < score) {
            alpha = score;

            control.setPv(ply, createFullMove(move));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::visitChildren() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    NodeAb child{*this};

    const Move& move = control.getPv(0)[ply];
    if (isLegalMove(move)) {
        CUTOFF (child.visit(move));
    }

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            CUTOFF (child.visit({from, to}));
        }
    }

    return NodeControl::Continue;
}

Score NodeAb::staticEval()
{
    return Position::evaluate().clamp();
}

Move NodeAb::createFullMove(Square from, Square to) const {
    Color color = control.info.positionFen.getColorToMove() << ply;
    return Move{from, to, isSpecial(from, to), color, control.info.positionFen.getChessVariant()};
}
