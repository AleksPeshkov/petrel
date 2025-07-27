#include "NodePerftTT.hpp"
#include "SearchRoot.hpp"
#include "TtPerft.hpp"

NodePerftTT::NodePerftTT (NodePerftTT* n) : Node{n->root}, parent{n}, draft{n->draft-1} {}
NodePerftTT::NodePerftTT (const PositionMoves& p, SearchRoot& r, Ply d) : Node{p, r}, parent{nullptr}, draft{d} {}

NodeControl NodePerftTT::visitChildren() {
    NodePerftTT node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_ABORT (child->visit(from, to));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodePerftTT::visit(Square from, Square to) {
    switch (draft) {
        case 0:
            parent->perft += 1;
            return NodeControl::Continue;

        case 1:
            RETURN_IF_ABORT (root.countNode());
            makeMoveNoZobrist(parent, from, to);
            parent->perft += moves.popcount();
            return NodeControl::Continue;

        default: {
            assert(draft >= 2);
            RETURN_IF_ABORT (root.countNode());
            makeMove(parent, from, to);

            auto n = static_cast<TtPerft&>(root.tt).get(getZobrist(), draft - 2);
            if (n != NodeCountNone) {
                perft = n;
            } else {
                perft = 0;
                RETURN_IF_ABORT(visitChildren());
                static_cast<TtPerft&>(root.tt).set(getZobrist(), draft - 2, perft);
            }
            parent->perft += perft;
        }
    }

    return NodeControl::Continue;
}
