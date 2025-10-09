#include "Repetitions.hpp"

void test_repetition_mask() {
    RepetitionHash m1;
    assert (!m1.has(ZArg{1}) && "False positive");

    RepetitionHash m2{m1, ZArg{1}};
    assert (m2.has(ZArg{1}) && "False negative");
    assert (!m2.has(ZArg{2}) && "False positive");
    assert (!m2.has(ZArg{3}) && "False positive");

    RepetitionHash m3{m2, ZArg{2}};
    assert (m3.has(ZArg{1}) && "False negative");
    assert (m3.has(ZArg{2}) && "False negative");
    assert (!m3.has(ZArg{3}) && "False positive");
}

void test_no_repetition() {
    RepetitionsSide reps;

    reps.push(ZArg{1});
    reps.push(ZArg{2});
    reps.push(ZArg{3});
    reps.push(ZArg{4});

    reps.normalize();

    assert (reps.size() == 0 && "Invalid count of repetitions");
    assert (!reps.has(ZArg{1}) && "Invalid repetition detected");
    assert (!reps.has(ZArg{2}) && "Invalid repetition detected");
}

void test_basic_repetition() {
    RepetitionsSide reps;
    assert (reps.size() == 0 && "Invalid count of repetitions");

    reps.push(ZArg{1});
    reps.push(ZArg{2});
    reps.push(ZArg{1});
    reps.push(ZArg{3});

    assert (reps.size() == 4 && "Invalid count of repetitions");

    reps.normalize();

    assert (reps.size() == 1 && "Invalid count of repetitions");
    assert (reps.has(ZArg{1}) && "Valid repetition not detected");
    assert (!reps.has(ZArg{2}) && "Invalid repetition detected");
    assert (!reps.has(ZArg{3}) && "Invalid repetition detected");
}

void test_multiple_repetitions() {
    RepetitionsSide reps;

    reps.push(ZArg{1});
    reps.push(ZArg{65});
    reps.push(ZArg{1});
    reps.push(ZArg{3});
    reps.push(ZArg{65});
    reps.push(ZArg{3});
    reps.push(ZArg{4});

    reps.normalize();

    assert (reps.size() == 3 && "Invalid count of repetitions");
    assert (reps.has(ZArg{1}) && "Valid repetition not detected");
    assert (reps.has(ZArg{65}) && "Valid repetition not detected");
    assert (reps.has(ZArg{3}) && "Valid repetition not detected");
    assert (!reps.has(ZArg{4}) && "Invalid repetition detected");
}

void test_edge_cases() {
    RepetitionsSide reps;

    // Zero entries
    assert (!reps.has(ZArg{1}) && "Invalid repetition detected");

    // One entry
    reps.push(ZArg{1});
    reps.normalize();
    assert (!reps.has(ZArg{1}) && "Invalid repetition detected");

    // Max entries (50)
    for (int i = 0; i < 50; ++i) {
        reps.push(ZArg{static_cast<Z::_t>(i % 2 ? 1 : 65)}); // Alternate between two ZArgs
    }
    reps.normalize();
    assert (reps.size() == 2 && "Invalid count of repetitions");
    assert (reps.has(ZArg{1}) && "Valid repetition not detected");
    assert (!reps.has(ZArg{2}) && "Invalid repetition detected");
    assert (reps.has(ZArg{65}) && "Valid repetition not detected");

    // Buffer wrap-around
    reps.clear();
    for (int i = 0; i < 60; ++i) {
        reps.push(ZArg{static_cast<Z::_t>(i % 40)});
    }
    reps.normalize();
    assert (reps.size() == 10 && "Invalid count of repetitions");
    assert (!reps.has(ZArg{1}) && "Valid repetition not detected");
    assert (reps.has(ZArg{11}) && "Valid repetition not detected");
    assert (!reps.has(ZArg{21}) && "Invalid repetition detected");
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
