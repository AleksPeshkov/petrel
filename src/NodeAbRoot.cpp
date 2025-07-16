#include "NodeAbRoot.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "SearchLimit.hpp"

NodeAbRoot::NodeAbRoot (const SearchLimit& limit, SearchControl& control):
    NodeAb{limit.positionMoves, control}, depthLimit{limit.depth}
{}

NodeControl NodeAbRoot::visitChildren() {
    auto rootMoves = cloneMoves();
    auto rootMovesCount = movesCount();

    for (draft = 1; draft <= depthLimit; ++draft) {
        setMoves(rootMoves, rootMovesCount);
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
