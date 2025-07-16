#include "NodeAb.hpp"
#include "out.hpp"
#include "SearchControl.hpp"
#include "UciSearchInfo.hpp"

NodeControl NodeAb::visit(Move move) {
    RETURN_IF_ABORT (control.countNode());

    score = Score::None;
    alpha = -parent.beta;
    beta = -parent.alpha;

    draft = parent.draft > 0 ? parent.draft-1 : 0;
    makeMove(parent, move);

    if (movesCount() == 0 || draft == 0) {
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

            control.setPv(ply, move);
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::visitChildren() {
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
