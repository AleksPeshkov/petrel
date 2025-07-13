#include "NodePerftRoot.hpp"
#include "NodePerftTT.hpp"
#include "NodePerftLeaf.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "SearchLimit.hpp"

NodeControl NodePerftRoot::visitChildren() {
    switch (draft) {
        case 1:
            perft = movesCount();
            break;

        case 2:
            RETURN_IF_ABORT ( static_cast<NodePerftLeaf>(*this).visitChildren() );
            break;

        default:
            assert (draft >= 3);
            RETURN_IF_ABORT ( static_cast<NodePerftTT>(*this).visitChildren() );
    }

    control.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftRootDepth::visitChildren() {
    NodePerftRoot::visitChildren();

    control.perft_finish();
    return NodeControl::Continue;
}

NodeControl NodePerftRootIterative::visitChildren() {
    for (draft = 1; draft <= MaxDepth; ++draft) {
        auto c = NodePerftRoot(*this, control, draft).visitChildren();
        if (c != NodeControl::Continue) { break; }

        control.newIteration();
    }

    control.perft_finish();
    return NodeControl::Continue;
}

NodeControl NodePerftDivide::visit(Square from, Square to) {
    switch (draft) {
        case 0:
            perft = 1;
            break;

        case 1:
            makeMove(parent, from, to);
            perft = movesCount();
            break;

        default: {
            assert (draft >= 2);
            setZobrist(parent, from, to);

            auto n = control.tt().get(getZobrist(), draft-2);
            if (n != NodeCountNone) {
                perft = n;
            }
            else {
                makeMove(parent, from, to);

                switch (draft) {
                    case 2:
                        RETURN_IF_ABORT ( static_cast<NodePerftLeaf>(*this).visitChildren() );
                        break;

                    default:
                        assert (draft >= 3);
                        RETURN_IF_ABORT ( static_cast<NodePerftTT>(*this).visitChildren() );
                }

                control.tt().set(getZobrist(), draft-2, perft);
            }
        }
    }

    ++moveCount;
    control.perft_currmove(moveCount, {parent, from, to}, perft);

    updateParentPerft();
    return NodeControl::Continue;
}

NodeControl NodePerftDivide::visitChildren() {
    perft = 0;

    auto _moves = cloneMoves();
    auto child = NodePerftDivide(*this);

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : _moves[pi]) {
            RETURN_IF_ABORT (beforeVisit());
            _moves.clear(pi, to);

            RETURN_IF_ABORT (child.visit(from, to));
        }
    }

    control.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftDivideDepth::visitChildren() {
    NodePerftDivide::visitChildren();

    control.perft_finish();
    return NodeControl::Continue;
}

NodeControl NodePerftDivideIterative::visitChildren() {
    for (draft = 1; draft <= MaxDepth; ++draft) {
        auto c = NodePerftDivide(*this, control, draft).visitChildren();
        if (c != NodeControl::Continue) { break; }

        control.newIteration();
    }

    control.perft_finish();
    return NodeControl::Continue;
}
