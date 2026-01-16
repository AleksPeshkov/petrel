#include "history.hpp"

void test_history_moves() {
    Color c{White};
    PieceType ty{Knight};

    HistoryMoves<1> hm;
    hm.set(c, HistoryMove{ty, Square{B8}, Square{C6}}, HistoryMove{PieceType{Pawn}, Square{E2}, Square{E4}});
    assert (( hm.get(0, c, HistoryMove{ty, Square{B8}, Square{C6}}) == HistoryMove{PieceType{Pawn}, Square{E2}, Square{E4}} ));
}

namespace TestHistoryMoves {
    void test() {
        test_history_moves();
    }
}
