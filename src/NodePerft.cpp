#include "NodePerft.hpp"
#include "NodePerftTT.hpp"
#include "SearchControl.hpp"

NodeControl NodePerft::visitChildren() {
    for (Pi pi : parent[My].pieces()) {
        Square from = parent[My].squareOf(pi);

        for (Square to : static_cast<NodePerftTT&>(parent).moves[pi]) {
            RETURN_IF_ABORT (visit(from, to));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodePerft::visit(Square from, Square to) {
    RETURN_IF_ABORT (control.countNode());
    makeMove(parent, from, to);

    //updateParentPerft:
    static_cast<NodePerftTT&>(parent).perft += movesCount();

    return NodeControl::Continue;
}
