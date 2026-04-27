#include "history.hpp"

void test_history_moves() {
    Color c{White};

    HistoryMoves<2> hm;
    HistoryMove move1{Square{B8}, Square{C6}, CanBeKiller::No, HistoryType{HistoryQN}};
    HistoryMove move2{Square{E2}, Square{E4}, CanBeKiller::Yes, HistoryType{HistoryPawn}};
    HistoryMove move3{Square{H1}, Square{E1}, CanBeKiller::Yes, HistoryType{HistoryRB}};

    hm.set(c, move1, move2);
    assert ( hm.get(HistoryMoves<2>::Index{0}, c, move1) == move2);
    hm.set(c, move1, move3);
    assert ( hm.get(HistoryMoves<2>::Index{0}, c, move1) == move3);
    assert ( hm.get(HistoryMoves<2>::Index{1}, c, move1) == move2);
}

namespace TestHistoryMoves {
    void test() {
        test_history_moves();
    }
}
