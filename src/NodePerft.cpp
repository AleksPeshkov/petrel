#include "NodePerft.hpp"
#include "NodePerftTT.hpp"
#include "SearchControl.hpp"

NodePerft::NodePerft (NodePerftTT& n) : Node{n.control}, parent{n} {}
NodePerft::NodePerft (const PositionMoves& p, SearchControl& c) : Node{p, c}, parent{static_cast<NodePerftTT&>(*this)} {}

NodeControl NodePerft::visitChildren() {
    for (Pi pi : parent[My].pieces()) {
        Square from = parent[My].squareOf(pi);

        for (Square to : parent.moves[pi]) {
            RETURN_IF_ABORT (visit(from, to));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodePerft::visit(Square from, Square to) {
    RETURN_IF_ABORT (control.countNode());
    makeMove(parent, from, to);

    //updateParentPerft:
    assert (&parent != this);
    parent.perft += movesCount();

    return NodeControl::Continue;
}
