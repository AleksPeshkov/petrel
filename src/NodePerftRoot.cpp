#include "NodePerftRoot.hpp"
#include "NodePerftTT.hpp"
#include "NodePerft.hpp"
#include "Move.hpp"
#include "SearchControl.hpp"
#include "SearchLimit.hpp"

NodeControl NodePerftRoot::visitRoot() {
    switch (draft) {
        case 1:
            perft = movesCount();
            break;

        case 2:
            RETURN_IF_ABORT ( static_cast<NodePerft>(*this).visitChildren() );
            break;

        default:
            assert (draft >= 3);
            RETURN_IF_ABORT ( static_cast<NodePerftTT>(*this).visitChildren() );
    }

    control.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftRootDepth::visitChildren() {
    NodePerftRoot::visitRoot();

    control.perft_finish();
    return NodeControl::Continue;
}

NodeControl NodePerftRootIterative::visitChildren() {
    auto rootMoves = cloneMoves();
    auto rootMovesCount = movesCount();

    for (draft = 1; draft <= MaxDepth; ++draft) {
        control.newIteration();
        setMoves(rootMoves, rootMovesCount);
        BREAK_IF_ABORT (( NodePerftRoot(*this, control, draft).visitRoot() ));
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
                        RETURN_IF_ABORT ( static_cast<NodePerft>(*this).visitChildren() );
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

NodeControl NodePerftDivide::divideChildren() {
    perft = 0;
    NodePerftDivide child{*this};

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_ABORT (child.visit(from, to));
        }
    }

    control.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftDivideDepth::visitChildren() {
    control.newIteration();
    NodePerftDivide::divideChildren();

    control.perft_finish();
    return NodeControl::Continue;
}

NodeControl NodePerftDivideIterative::visitChildren() {
    auto rootMoves = cloneMoves();
    auto rootMovesCount = movesCount();

    for (draft = 1; draft <= MaxDepth; ++draft) {
        control.newIteration();
        setMoves(rootMoves, rootMovesCount);
        BREAK_IF_ABORT (( NodePerftDivide(*this, control, draft).divideChildren() ));
    }

    control.perft_finish();
    return NodeControl::Continue;
}
