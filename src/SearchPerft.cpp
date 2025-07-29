#include "SearchPerft.hpp"
#include "SearchRoot.hpp"
#include "TtPerft.hpp"

void PerftThread::run() {
    PerftNode rootNode{root.position, root, depth};
    rootNode.visitRoot(isDivide);
}

ReturnStatus PerftNode::visitRoot(bool isDivide) {
    ReturnStatus status = isDivide ? searchDivide() : searchMoves();;

    if (status != ReturnStatus::Abort) {
        root.perft_depth(draft, perft);
    }

    root.perft_finish();
    return status;
}

ReturnStatus PerftNode::searchDivide() {
    PerftNode node{this};
    const auto child = &node;

    index_t moveCount = 0;
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            auto previousPerft = perft;

            RETURN_IF_ABORT (child->visit(from, to));

            UciMove move{from, to, isSpecial(from, to), root.position.getColorToMove(), root.position.getChessVariant()};
            root.perft_currmove(++moveCount, move, perft - previousPerft);
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus PerftNode::searchMoves() {
    PerftNode node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_ABORT (child->visit(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus PerftNode::visit(Square from, Square to) {
    switch (draft) {
        case 0:
            parent->perft += 1;
            return ReturnStatus::Continue;

        case 1:
            RETURN_IF_ABORT (root.countNode());
            makeMoveNoZobrist(parent, from, to);
            parent->perft += moves.popcount();
            return ReturnStatus::Continue;

        default: {
            assert(draft >= 2);
            RETURN_IF_ABORT (root.countNode());
            makeMove(parent, from, to);

            auto n = static_cast<TtPerft&>(root.tt).get(zobrist, draft - 2);
            if (n != NodeCountNone) {
                perft = n;
            } else {
                perft = 0;
                RETURN_IF_ABORT(searchMoves());
                static_cast<TtPerft&>(root.tt).set(zobrist, draft - 2, perft);
            }
            parent->perft += perft;
        }
    }

    return ReturnStatus::Continue;
}
