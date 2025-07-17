#include "NodeAbRoot.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "SearchLimit.hpp"

NodeAbRoot::NodeAbRoot (const SearchLimit& limit, SearchControl& searchControl):
    NodeAb{limit.positionMoves, searchControl}, depthLimit{limit.depth}
{}

NodeControl NodeAbRoot::visitChildren() {
    auto rootMoves = cloneMoves();
    auto rootMovesCount = movesCount();

    for (draft = 1; draft <= depthLimit; ++draft) {
        control.newIteration();
        setMoves(rootMoves, rootMovesCount);
        score = NoScore;
        alpha = MinusInfinity;
        beta = PlusInfinity;
        BREAK_IF_ABORT ( NodeAb::visitChildren() );
        control.infoDepth(draft, score);;
    }

    control.bestmove(score);
    return NodeControl::Abort;
}
