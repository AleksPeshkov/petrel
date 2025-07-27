#include "NodeAbRoot.hpp"
#include "SearchRoot.hpp"
#include "UciGoLimit.hpp"

NodeAbRoot::NodeAbRoot (const UciGoLimit& l, SearchRoot& r):
    NodeAb{l.positionMoves, r}, depthLimit{l.depth}
{}

NodeControl NodeAbRoot::visitChildren() {
    auto rootMovesClone = moves;

    for (draft = 1; draft <= depthLimit; ++draft) {
        moves = rootMovesClone;
        score = NoScore;
        alpha = MinusInfinity;
        beta = PlusInfinity;
        BREAK_IF_ABORT ( NodeAb::visitChildren() );
        root.infoIterationEnd(draft);
        root.newIteration();
    }

    root.bestmove();
    return NodeControl::Continue;
}
