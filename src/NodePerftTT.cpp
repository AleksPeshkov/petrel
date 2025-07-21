#include "NodePerftTT.hpp"
#include "SearchControl.hpp"
#include "TtPerft.hpp"

NodePerftTT::NodePerftTT (NodePerftTT& n) : Node{n.control}, parent{n}, draft{n.draft-1} {}
NodePerftTT::NodePerftTT (const PositionMoves& p, SearchControl& c, Ply d) : Node{p, c}, parent{*this}, draft{d} {}

NodeControl NodePerftTT::visitChildren() {
    NodePerftTT child{*this};

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_ABORT (child.visit(from, to));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodePerftTT::visit(Square from, Square to) {
    switch (draft) {
        case 0:
            parent.perft += 1;
            return NodeControl::Continue;

        case 1:
            RETURN_IF_ABORT (control.countNode());
            makeMove(parent, from, to);
            parent.perft += movesCount();
            return NodeControl::Continue;

        default: {
            assert(draft >= 2);
            setZobrist(parent, from, to);

            auto n = static_cast<TtPerft&>(control.tt).get(getZobrist(), draft - 2);
            if (n != NodeCountNone) {
                perft = n;
            } else {
                RETURN_IF_ABORT (control.countNode());
                makeMove(parent, from, to);

                perft = 0;
                RETURN_IF_ABORT(visitChildren());
                static_cast<TtPerft&>(control.tt).set(getZobrist(), draft - 2, perft);
            }
            parent.perft += perft;
        }
    }

    return NodeControl::Continue;
}
