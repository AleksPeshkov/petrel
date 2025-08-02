#include "NodePerft.hpp"
#include "NodeRoot.hpp"
#include "TtPerft.hpp"
#include "Uci.hpp"

NodePerft::NodePerft (NodeRoot& r, Ply d) : PositionMoves{r}, root{r}, parent{nullptr}, draft(d) {}

void NodePerft::visitRoot(bool isDivide) {
    ReturnStatus status = isDivide ? searchDivide() : searchMoves();

    if (status != ReturnStatus::Stop) {
        root.uci.perft_depth(draft, perft);
    }

    root.uci.perft_finish();
}

ReturnStatus NodePerft::searchDivide() {
    NodePerft node{this};
    const auto child = &node;

    index_t moveCount = 0;
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            auto previousPerft = perft;

            RETURN_IF_STOP (child->visit(from, to));

            UciMove move{from, to, isSpecial(from, to), root.colorToMove(), root.uci.chessVariant()};
            root.uci.perft_currmove(++moveCount, move, perft - previousPerft);
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::searchMoves() {
    NodePerft node{this};
    const auto child = &node;

    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_IF_STOP (child->visit(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus NodePerft::visit(Square from, Square to) {
    switch (draft) {
        case 0:
            parent->perft += 1;
            return ReturnStatus::Continue;

        case 1:
            RETURN_IF_STOP (root.countNode());
            makeMoveNoZobrist(parent, from, to);
            parent->perft += moves.popcount();
            return ReturnStatus::Continue;

        default: {
            assert(draft >= 2);
            makeZobrist(parent, from, to);
            root.tt.prefetch(zobrist, 64);
            RETURN_IF_STOP (root.countNode());
            makeMoveNoZobrist(parent, from, to);

            auto n = static_cast<TtPerft&>(root.tt).get(zobrist, draft - 2);
            if (n != NodeCountNone) {
                perft = n;
            } else {
                perft = 0;
                RETURN_IF_STOP(searchMoves());
                static_cast<TtPerft&>(root.tt).set(zobrist, draft - 2, perft);
            }
            parent->perft += perft;
        }
    }

    return ReturnStatus::Continue;
}
