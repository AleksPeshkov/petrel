#include "NodeAbAll.hpp"
#include "NodeAbCut.hpp"
#include "SearchControl.hpp"

NodeControl NodeAbAll::visit(Square from, Square to) {
    auto& p = static_cast<NodeAbCut&>(parent);

    playMove(p, from, to);

    if (getMovesCount() == 0 || draft == 0) {
        bestScore = getStaticEval();
    }
    else {
        bestScore = Score::Minimum;
        NodeControl result = NodeAbCut(*this).visitChildren();

        //the search is interrupted so exit immediately
        if (result == NodeControl::Abort && control.isAborted()) {
            return NodeControl::Abort;
        }
    }

    if (-bestScore > p.bestScore) {
        p.bestScore = -bestScore;
        control.createPv(Move{p, from, to});

        if (p.alpha < p.bestScore) {
            p.alpha = p.bestScore;

            if (p.alpha >= p.beta) {
                //beta cut off
                return NodeControl::Abort;
            }
        }
    }

    return NodeControl::Continue;
}
