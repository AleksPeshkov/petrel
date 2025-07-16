#include "NodeAbRoot.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "SearchLimit.hpp"

NodeAbRoot::NodeAbRoot (const SearchLimit& limit, SearchControl& searchControl):
    NodeAb{limit.positionMoves, searchControl}, depthLimit{limit.depth ? limit.depth : MaxDepth}
{}

NodeControl NodeAbRoot::visitChildren() {
    auto rootMoves = cloneMoves();
    auto rootMovesCount = movesCount();

    for (draft = 1; draft <= depthLimit; ++draft) {
        control.newIteration();
        setMoves(rootMoves, rootMovesCount);
        score = Score::None;
        alpha = Score::Minimum;
        beta = Score::Maximum;
        BREAK_IF_ABORT ( NodeAb::visitChildren() );
        control.infoDepth(draft, score);;
    }

    control.bestmove(score);
    return NodeControl::Abort;
}
