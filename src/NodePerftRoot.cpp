#include "NodePerftRoot.hpp"
#include "UciSearchInfo.hpp"

NodeControl NodePerftRoot::visitRoot() {
    if (!isDivide) {
        RETURN_IF_ABORT (NodePerftTT::visitChildren());
        control.perft_depth(draft, perft);
        return NodeControl::Continue;
    }

    index_t moveCount = 0;

    NodePerftTT child(*this);
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : moves[pi]) {
            auto previousPerft = perft;
            RETURN_IF_ABORT (child.visit(from, to));
            Move move{from, to, isSpecial(from, to), control.info.positionFen.getColorToMove(), control.info.positionFen.getChessVariant()};
            control.perft_currmove(++moveCount, move, perft - previousPerft);
        }
    }

    control.perft_depth(draft, perft);
    return NodeControl::Continue;
}

NodeControl NodePerftRoot::visitChildren() {
    visitRoot();
    control.perft_finish();
    return NodeControl::Continue;
}
