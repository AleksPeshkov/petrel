#include "history.hpp"

void test_repetition_mask() {
    RepetitionHash m1;
    assert (!m1.has(Z{1}) && "False positive");

    RepetitionHash m2{m1, Z{1}};
    assert (m2.has(Z{1}) && "False negative");
    assert (!m2.has(Z{2}) && "False positive");
    assert (!m2.has(Z{3}) && "False positive");

    RepetitionHash m3{m2, Z{2}};
    assert (m3.has(Z{1}) && "False negative");
    assert (m3.has(Z{2}) && "False negative");
    assert (!m3.has(Z{3}) && "False positive");
}

void test_no_repetition() {
    RepetitionsSide reps;

    reps.push(Z{1});
    reps.push(Z{2});
    reps.push(Z{3});
    reps.push(Z{4});

    reps.normalize();

    assert (reps.size() == 0 && "Invalid count of repetitions");
    assert (!reps.has(Z{1}) && "Invalid repetition detected");
    assert (!reps.has(Z{2}) && "Invalid repetition detected");
}

void test_basic_repetition() {
    RepetitionsSide reps;
    assert (reps.size() == 0 && "Invalid count of repetitions");

    reps.push(Z{1});
    reps.push(Z{2});
    reps.push(Z{1});
    reps.push(Z{3});

    assert (reps.size() == 4 && "Invalid count of repetitions");

    reps.normalize();

    assert (reps.size() == 1 && "Invalid count of repetitions");
    assert (reps.has(Z{1}) && "Valid repetition not detected");
    assert (!reps.has(Z{2}) && "Invalid repetition detected");
    assert (!reps.has(Z{3}) && "Invalid repetition detected");
}

void test_multiple_repetitions() {
    RepetitionsSide reps;

    reps.push(Z{1});
    reps.push(Z{65});
    reps.push(Z{1});
    reps.push(Z{3});
    reps.push(Z{65});
    reps.push(Z{3});
    reps.push(Z{4});

    reps.normalize();

    assert (reps.size() == 3 && "Invalid count of repetitions");
    assert (reps.has(Z{1}) && "Valid repetition not detected");
    assert (reps.has(Z{65}) && "Valid repetition not detected");
    assert (reps.has(Z{3}) && "Valid repetition not detected");
    assert (!reps.has(Z{4}) && "Invalid repetition detected");
}

void test_edge_cases() {
    RepetitionsSide reps;

    // Zero entries
    assert (!reps.has(Z{1}) && "Invalid repetition detected");

    // One entry
    reps.push(Z{1});
    reps.normalize();
    assert (!reps.has(Z{1}) && "Invalid repetition detected");

    // Max entries (50)
    for (int i = 0; i < 50; ++i) {
        reps.push(Z{static_cast<Z::_t>(i % 2 ? 1 : 65)}); // Alternate between two ZArgs
    }
    reps.normalize();
    assert (reps.size() == 2 && "Invalid count of repetitions");
    assert (reps.has(Z{1}) && "Valid repetition not detected");
    assert (!reps.has(Z{2}) && "Invalid repetition detected");
    assert (reps.has(Z{65}) && "Valid repetition not detected");

    // Buffer wrap-around
    reps.clear();
    for (int i = 0; i < 60; ++i) {
        reps.push(Z{static_cast<Z::_t>(i % 40)});
    }
    reps.normalize();
    assert (reps.size() == 10 && "Invalid count of repetitions");
    assert (!reps.has(Z{1}) && "Valid repetition not detected");
    assert (reps.has(Z{11}) && "Valid repetition not detected");
    assert (!reps.has(Z{21}) && "Invalid repetition detected");
}

namespace TestRepetitions {
    void test() {
        test_repetition_mask();
        test_no_repetition();
        test_basic_repetition();
        test_multiple_repetitions();
        test_edge_cases();
    }
}
