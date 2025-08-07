#include "NodePerft.hpp"
#include "NodeRoot.hpp"
#include "TtPerft.hpp"
#include "Uci.hpp"

NodePerft::NodePerft (NodeRoot& r, Ply d) : PositionMoves{r}, parent{nullptr}, root{r}, draft{d} {}

ReturnStatus NodePerft::visitRoot() {
    root.newSearch();
    root.nodeCounter = {};

    NodePerft node{this};
    const auto child = &node;

    index_t moveCount = 0;
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            auto previousPerft = perft;

            RETURN_IF_STOP (child->visitMove(from, to));

            UciMove move{from, to, isSpecial(from, to), root.colorToMove(), root.uci.chessVariant()};
            root.uci.perft_currmove(++moveCount, move, perft - previousPerft);
        }
    }

    root.uci.perft_depth(draft, perft);
    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visit() {
    NodePerft node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_STOP (child->visitMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visitMove(Square from, Square to) {
    switch (draft) {
        case 0:
            perft = 1;
            break;

        case 1:
            RETURN_IF_STOP (root.countNode());
            makeMoveNoZobrist(parent, from, to);
            perft = moves.popcount();
            break;

        default: {
            assert(draft >= 2);
            makeZobrist(parent, from, to);
            root.tt.prefetch(zobrist, 64);

            RETURN_IF_STOP (root.countNode());
            makeMoveNoZobrist(parent, from, to);

            perft = static_cast<TtPerft&>(root.tt).get(zobrist, draft - 2);

            if (perft == NodeCountNone) {
                perft = 0;
                RETURN_IF_STOP(visit());
                static_cast<TtPerft&>(root.tt).set(zobrist, draft - 2, perft);
            }
        }
    }

    parent->perft += perft;
    return ReturnStatus::Continue;
}
