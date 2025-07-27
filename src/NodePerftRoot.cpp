#include "NodePerftRoot.hpp"

NodeControl NodePerftRoot::visitRoot() {
    if (!isDivide) {
        RETURN_IF_ABORT (NodePerftTT::visitChildren());
        root.perft_depth(draft, perft);
        return NodeControl::Continue;
    }

    index_t moveCount = 0;

    NodePerftTT node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : moves[pi]) {
            auto previousPerft = perft;
            RETURN_IF_ABORT (child->visit(from, to));

            UciMove move{from, to, isSpecial(from, to), root.position.getColorToMove(), root.position.getChessVariant()};
            root.perft_currmove(++moveCount, move, perft - previousPerft);
        }
    }

    root.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftRoot::visitChildren() {
    visitRoot();
    root.perft_finish();
    return NodeControl::Continue;
}
