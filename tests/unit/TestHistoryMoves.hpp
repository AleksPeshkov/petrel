#include "history.hpp"

void test_history_moves() {
    Color c{White};

    ContinuationMoves<2> hm;
    HistoryMove move1{Square{B8}, Square{C6}, HistoryType{HistoryQN}};
    HistoryMove move2{Square{E2}, Square{E4}, HistoryType{HistorySpecial}};
    HistoryMove move3{Square{H1}, Square{E1}, HistoryType{HistoryRB}};

    hm.set(c, move1, move2);
    assert ( hm.get(ContinuationMoves<2>::Index{0}, c, move1) == move2);
    hm.set(c, move1, move3);
    assert ( hm.get(ContinuationMoves<2>::Index{0}, c, move1) == move3);
    assert ( hm.get(ContinuationMoves<2>::Index{1}, c, move1) == move2);
}

namespace TestHistoryMoves {
    void test() {
        test_history_moves();
    }
}
