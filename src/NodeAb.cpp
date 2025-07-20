#include "NodeAbPv.hpp"
#include "SearchControl.hpp"

NodeControl NodeAb::visit(Square from, Square to) {
    auto& p = static_cast<NodeAb&>(parent);

    playMove(p, from, to);

    if (getMovesCount() == 0 || draft == 0) {
        bestScore = getStaticEval();
    }
    else {
        bestScore = Score::Minimum;
        NodeControl result = NodeAb(*this).visitChildren();

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
