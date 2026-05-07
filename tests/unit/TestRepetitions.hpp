#include "history.hpp"

class Zt : public Z {
public:
    explicit Zt (_t v) : Z{v} {}
};

void test_repetition_mask() {
    RepHash m1;
    assert (!m1.has(Zt{1}) && "False positive");

    RepHash m2{m1, Zt{1}};
    assert (m2.has(Zt{1}) && "False negative");
    assert (!m2.has(Zt{2}) && "False positive");
    assert (!m2.has(Zt{3}) && "False positive");

    RepHash m3{m2, Zt{2}};
    assert (m3.has(Zt{1}) && "False negative");
    assert (m3.has(Zt{2}) && "False negative");
    assert (!m3.has(Zt{3}) && "False positive");
}

void test_no_repetition() {
    RepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{3});
    reps.push(Zt{4});

    reps.normalize();

    assert (reps.size() == 0 && "Invalid count of repetitions");
    assert (!reps.has(Zt{1}) && "Invalid repetition detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
}

void test_basic_repetition() {
    RepSide reps;
    assert (reps.size() == 0 && "Invalid count of repetitions");

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{1});
    reps.push(Zt{3});

    assert (reps.size() == 4 && "Invalid count of repetitions");

    reps.normalize();

    assert (reps.size() == 1 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
    assert (!reps.has(Zt{3}) && "Invalid repetition detected");
}

void test_multiple_repetitions() {
    RepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{65});
    reps.push(Zt{1});
    reps.push(Zt{3});
    reps.push(Zt{65});
    reps.push(Zt{3});
    reps.push(Zt{4});

    reps.normalize();

    assert (reps.size() == 3 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (reps.has(Zt{65}) && "Valid repetition not detected");
    assert (reps.has(Zt{3}) && "Valid repetition not detected");
    assert (!reps.has(Zt{4}) && "Invalid repetition detected");
}

void test_edge_cases() {
    RepSide reps;

    // Zero entries
    assert (!reps.has(Zt{1}) && "Invalid repetition detected");

    // One entry
    reps.push(Zt{1});
    reps.normalize();
    assert (!reps.has(Zt{1}) && "Invalid repetition detected");

    // Max entries (50)
    for (int i = 0; i < 50; ++i) {
        reps.push(Zt{static_cast<Z::_t>(i % 2 ? 1 : 65)}); // Alternate between two ZArgs
    }
    reps.normalize();
    assert (reps.size() == 2 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
    assert (reps.has(Zt{65}) && "Valid repetition not detected");

    // Buffer wrap-around
    reps.clear();
    for (int i = 0; i < 60; ++i) {
        reps.push(Zt{static_cast<Z::_t>(i % 40)});
    }
    reps.normalize();
    assert (reps.size() == 10 && "Invalid count of repetitions");
    assert (!reps.has(Zt{1}) && "Valid repetition not detected");
    assert (reps.has(Zt{11}) && "Valid repetition not detected");
    assert (!reps.has(Zt{21}) && "Invalid repetition detected");
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
