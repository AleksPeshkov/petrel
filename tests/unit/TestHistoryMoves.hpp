#include "history.hpp"

void test_history_moves() {
    Color c{White};
    PieceType ty{Knight};
    Square sq{C6};

    HistoryMoves<1> hm;
    hm.set(c, ty, sq, HistoryMove{PieceType{Pawn}, E2, E4});
    assert (( hm.get(0, c, ty, sq) == HistoryMove{PieceType{Pawn}, E2, E4} ));
}

namespace TestHistoryMoves {
    void test() {
        test_history_moves();
    }
}
