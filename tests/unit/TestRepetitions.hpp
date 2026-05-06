#include "history.hpp"

class TestRepSide : public RepSide {
public:
    bool has(Z z) const {
        if (zHash().none(z)) { return false; }
        return RepSide::has(z);
    }

    constexpr auto ringCount() const { return ringCount_; }
    constexpr auto dupCount() const { return dupCount_; }
};

class Zt : public Z {
public:
    explicit Zt (_t v) : Z{v} {}
};

void test_repetition_mask() {
    ZHash z1;
    assert (z1.none(Zt{1}) && "False positive");

    ZHash z2{z1, Zt{1}};
    assert (!z2.none(Zt{1}) && "False negative");
    assert (z2.none(Zt{2}) && "False positive");
    assert (z2.none(Zt{3}) && "False positive");

    ZHash z3{z2, Zt{2}};
    assert (!z3.none(Zt{1}) && "False negative");
    assert (!z3.none(Zt{2}) && "False negative");
    assert (z3.none(Zt{3}) && "False positive");
}

void test_no_repetition() {
    TestRepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{3});
    reps.push(Zt{4});

    assert (reps.ringCount() == 4 && "Invalid count of repetitions");
    assert (reps.dupCount() == 0 && "Invalid count of repetitions");

    reps.normalize();

    assert (reps.ringCount() == 4 && "Invalid count of repetitions");
    assert (reps.dupCount() == 0 && "Invalid count of repetitions");
    assert (!reps.has(Zt{1}) && "Invalid repetition detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
}

void test_basic_repetition() {
    TestRepSide reps;
    assert (reps.ringCount() == 0 && "Invalid count of repetitions");
    assert (reps.dupCount() == 0 && "Invalid count of repetitions");

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{1});
    reps.push(Zt{3});

    assert (reps.ringCount() == 4 && "Invalid count of repetitions");
    assert (reps.dupCount() == 0 && "Invalid count of repetitions");

    reps.normalize();

    assert (reps.ringCount() == 4 && "Invalid count of repetitions");
    assert (reps.dupCount() == 1 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
    assert (!reps.has(Zt{3}) && "Invalid repetition detected");
}

void test_multiple_repetitions() {
    TestRepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{65});
    reps.push(Zt{1});
    reps.push(Zt{3});
    reps.push(Zt{65});
    reps.push(Zt{3});
    reps.push(Zt{4});

    reps.normalize();

    assert (reps.dupCount() == 3 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (reps.has(Zt{65}) && "Valid repetition not detected");
    assert (reps.has(Zt{3}) && "Valid repetition not detected");
    assert (!reps.has(Zt{4}) && "Invalid repetition detected");
}

void test_edge_cases() {
    TestRepSide reps;

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
    assert (reps.ringCount() == 50 && "Invalid count of repetitions");
    assert (reps.dupCount() == 2 && "Invalid count of repetitions");
    assert (reps.has(Zt{1}) && "Valid repetition not detected");
    assert (!reps.has(Zt{2}) && "Invalid repetition detected");
    assert (reps.has(Zt{65}) && "Valid repetition not detected");

    // Buffer wrap-around
    reps.clear();
    for (int i = 0; i < 60; ++i) {
        reps.push(Zt{static_cast<Z::_t>(i % 40)});
    }
    reps.normalize();
    assert (reps.ringCount() == 50 && "Invalid count of repetitions");
    assert (reps.dupCount() == 10 && "Invalid count of repetitions");
    assert (!reps.has(Zt{1}) && "Valid repetition not detected");
    assert (reps.has(Zt{11}) && "Valid repetition not detected");
    assert (!reps.has(Zt{21}) && "Invalid repetition detected");
}

void test_zhash_early_exit() {
    TestRepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{3});
    reps.normalize();

    Zt z4{4};
    assert(reps.zHash().none(z4) && "zHash should not contain Zt{4}");
    assert(!reps.has(z4) && "Search should exit early and not find Zt{4}");
}

void test_no_false_positive_in_zhash() {
    TestRepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{1 + (1ULL << 40)});
    reps.push(Zt{2});
    reps.normalize();

    Zt z_fake{1 + (1ULL << 30)}; // same hash, not in list
    if (!reps.zHash().none(z_fake)) {
        assert(!reps.has(z_fake) && "False positive in has()");
    }
}

void test_push_wraparound_consistency() {
    TestRepSide reps;

    for (int i = 0; i < 100; ++i) {
        reps.push(Zt{static_cast<Z::_t>(i % 5)});
    }

    reps.normalize();

    auto h = reps.zHash();
    assert(h.none(Zt{5}) && "Zt{5} should not be in hash");
    assert(!h.none(Zt{0}) && "Zt{0} should be in hash");
    assert(!h.none(Zt{4}) && "Zt{4} should be in hash");
}

void test_normalize_order() {
    TestRepSide reps;

    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{3});
    reps.push(Zt{1});
    reps.push(Zt{2});
    reps.push(Zt{4});

    reps.normalize();

    assert(reps.has(Zt{1}) && "Zt{1} should be detected");
    assert(reps.has(Zt{2}) && "Zt{2} should be detected");
    assert(!reps.has(Zt{3}) && "Zt{3} should not repeat");
    assert(!reps.has(Zt{4}) && "Zt{4} should not repeat");
}

void test_rotate_with_repetition() {
    TestRepSide reps;

    for (int i = 0; i < 50; ++i) {
        reps.push(Zt{1});
    }
    for (int i = 0; i < 10; ++i) {
        reps.push(Zt{2});
    }

    reps.normalize();

    assert(reps.ringCount() == 50 && "Repetitions should be found after rotation");
    assert(reps.dupCount() == 2 && "Repetitions should be found after rotation");
    assert(reps.has(Zt{1}) && "repetition should be detected");
    assert(reps.has(Zt{2}) && "repetition should be detected");
}

namespace TestRepetitions {
    void test() {
        test_repetition_mask();
        test_no_repetition();
        test_basic_repetition();
        test_multiple_repetitions();
        test_edge_cases();

        // New tests
        test_zhash_early_exit();
        test_no_false_positive_in_zhash();
        test_push_wraparound_consistency();
        test_normalize_order();
        test_rotate_with_repetition();
    }
}
