#include "Evaluation.hpp"

void test_delta() {
    DeltaPrunning delta;
    assert (delta(Score{-100}) == Pawn);
    assert (delta(Score{0}) == Pawn);
    assert (delta(Score{100}) == Pawn);
    assert (delta(Score{250}) == Knight);
    assert (delta(Score{450}) == Rook);
    assert (delta(Score{750}) == Queen);
    assert (delta(Score{1500}) == King);
}

namespace Delta {
    void test() {
        test_delta();
    }
}
