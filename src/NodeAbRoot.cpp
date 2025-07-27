#include "NodeAbRoot.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "UciGoLimit.hpp"

NodeAbRoot::NodeAbRoot (const UciGoLimit& l, SearchControl& c):
    NodeAb{l.positionMoves, c}, depthLimit{l.depth}
{}

NodeControl NodeAbRoot::visitChildren() {
    auto rootMovesClone = moves;

    for (draft = 1; draft <= depthLimit; ++draft) {
        moves = rootMovesClone;
        score = NoScore;
        alpha = MinusInfinity;
        beta = PlusInfinity;
        BREAK_IF_ABORT ( NodeAb::visitChildren() );
        control.infoIterationEnd(draft);
        control.newIteration();
    }

    control.bestmove();
    return NodeControl::Continue;
}
