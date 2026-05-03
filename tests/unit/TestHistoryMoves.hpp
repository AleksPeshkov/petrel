#include "history.hpp"
#include <random>

// -----------------------------------------------------------------------------
// ✅ Simple Random HistoryMove Generator
// -----------------------------------------------------------------------------

/// Generates a random HistoryMove (from, to, type) - no params needed
HistoryMove randomMove() {
    static std::mt19937 seed{12345}; // Fixed seed for reproducibility

    auto sq = [] {
        int sq = std::uniform_int_distribution(0, 63)(seed);
        return Square{static_cast<Square::_t>(sq)};
    };
    auto ht = [] {
        int ht = std::uniform_int_distribution<int>(0, 3)(seed);
        return HistoryType{static_cast<HistoryType::_t>(ht)};
    };
    return HistoryMove{TtMove{sq(), sq(), CanBeKiller::Yes}, ht()};
}

// -----------------------------------------------------------------------------
// ✅ Shared Testing Utility: expect_array_equals
// -----------------------------------------------------------------------------
// Compares std::array<HistoryMove, N> against expected list with detailed output
template <size_t Size>
void expect_array_equals(const std::array<HistoryMove, Size>& arr,
                         std::initializer_list<HistoryMove> expected,
                         const char* context = "expect_array_equals") {
    std::vector<HistoryMove> expVec(expected);
    bool failed = false;
    std::ostringstream msg;

    msg << "\n" << context << " failed - HistoryMove array mismatch:\n";

    for (size_t i = 0; i < Size; ++i) {
        bool hasExpected = (i < expVec.size());
        HistoryMove exp = hasExpected ? expVec[i] : HistoryMove{};
        HistoryMove act = arr[i];

        bool expValid = static_cast<bool>(exp);
        bool actValid = static_cast<bool>(act);

        if (expValid && actValid) {
            if (exp != act) {
                failed = true;
                msg << "  [" << i << "]: got " << act.from() << act.to();
                msg << ", expected " << exp.from() << exp.to() << "\n";
            }
        } else if (expValid != actValid) {
            failed = true;
            msg << "  [" << i << "]: got ";
            if (actValid) {
                msg << act.from() << act.to();
            } else {
                msg << "(null)";
            }
            msg << ", expected ";
            if (expValid) {
                msg << exp.from() << exp.to();
            } else {
                msg << "(null)";
            }
            msg << "\n";
        }
    }

    if (failed) {
        msg << "Test context: " << context << "\n";
        std::cerr << msg.str();
        assert(false && "HistoryMove array mismatch");
    }
}

// -----------------------------------------------------------------------------
// ✅ Test: ContinuationMoves basic behavior
// -----------------------------------------------------------------------------
void test_history_moves() {
    Color c{White};
    ContinuationMoves<2> hm;

    auto move1 = randomMove();
    auto move2 = randomMove();
    auto move3 = randomMove();

    hm.set(c, move1, move2);
    assert(hm.get(ContinuationMoves<2>::Index{0}, c, move1) == move2);

    hm.set(c, move1, move3);
    assert(hm.get(ContinuationMoves<2>::Index{0}, c, move1) == move3);
    assert(hm.get(ContinuationMoves<2>::Index{1}, c, move1) == move2);
}

// -----------------------------------------------------------------------------
// ✅ Test: insert_unique_compact with various array sizes
// -----------------------------------------------------------------------------
void test_insert_unique_compact() {
    constexpr size_t Size = 4;
    std::array<HistoryMove, Size> arr = {};
    auto clear = [&]() { std::memset(arr.data(), 0, sizeof(arr)); };

    auto A = randomMove();
    auto B = randomMove();
    auto C = randomMove();
    auto D = randomMove();
    auto E = randomMove();

    // === Test 1: Insert A,B,C → fill compactly up to Pos=2 ===
    clear();
    insert_unique_compact<2>(arr, A);
    insert_unique_compact<2>(arr, B);
    insert_unique_compact<2>(arr, C);
    expect_array_equals(arr, {A, B, C, HistoryMove{}}, "Test1: Insert A,B,C");

    // === Test 2: Insert duplicate C → no change ===
    insert_unique_compact<2>(arr, C);
    expect_array_equals(arr, {A, B, C, HistoryMove{}}, "Test2: Duplicate C");

    // === Test 3: Insert D → shift C to end ===
    insert_unique_compact<2>(arr, D);
    expect_array_equals(arr, {A, B, D, C}, "Test3: Insert D");

    // === Test 4: Re-insert B → already in [0,2), so no change ===
    insert_unique_compact<2>(arr, B);
    expect_array_equals(arr, {A, B, D, C}, "Test4: Re-insert B");

    // === Test 5: Insert E at Pos=1 → insert at index 1, shift right ===
    insert_unique_compact<1>(arr, E);
    expect_array_equals(arr, {A, E, B, D}, "Test5: Insert E at Pos=1");

    // === Test 6: Re-insert A → exists at 0 < Pos=1 → do nothing ===
    insert_unique_compact<1>(arr, A);
    expect_array_equals(arr, {A, E, B, D}, "Test6: Re-insert A");


    // ========================================================
    // ✅ Test: insert_unique with array size = 1
    // ========================================================

    std::array<HistoryMove, 1> small1{};
    auto clear1 = [&]() { std::memset(small1.data(), 0, sizeof(small1)); };

    auto X = randomMove();
    auto Y = randomMove();

    // --- S1.1: Insert first value at Pos=0 ---
    clear1();
    insert_unique_compact<0>(small1, X);
    expect_array_equals(small1, {X}, "S1.1: Insert X at Pos=0");

    // --- S1.2: Re-insert same value → already in [0,0) → no change ---
    insert_unique_compact<0>(small1, X);
    expect_array_equals(small1, {X}, "S1.2: Re-insert X");

    // --- S1.3: Insert new value → overwrite at 0 ---
    insert_unique_compact<0>(small1, Y);
    expect_array_equals(small1, {Y}, "S1.3: Insert Y");

    // --- S1.4: Insert X again → not present → insert at 0 ---
    insert_unique_compact<0>(small1, X);
    expect_array_equals(small1, {X}, "S1.4: Insert X again");


    // ========================================================
    // ✅ Test: insert_unique with array size = 2
    // ========================================================

    std::array<HistoryMove, 2> small2{};
    auto clear2 = [&]() { std::memset(small2.data(), 0, sizeof(small2)); };

    auto P = randomMove();
    auto Q = randomMove();
    auto R = randomMove();
    auto S = randomMove();

    // --- S2.1: Insert P,Q at Pos=1 → fill both slots ===
    clear2();
    insert_unique_compact<1>(small2, P);
    insert_unique_compact<1>(small2, Q);
    expect_array_equals(small2, {P, Q}, "S2.1: Insert P,Q at Pos=1");

    // --- S2.2: Re-insert P → already in [0,1) → no change ---
    insert_unique_compact<1>(small2, P);
    expect_array_equals(small2, {P, Q}, "S2.2: Re-insert P");

    // --- S2.3: Insert R → no empty in [0,1] → insert at 1, shift Q out ===
    insert_unique_compact<1>(small2, R);
    expect_array_equals(small2, {P, R}, "S2.3: Insert R");

    // --- S2.4: Insert S at Pos=0 → insert at front, shift P→1 ===
    insert_unique_compact<0>(small2, S);
    expect_array_equals(small2, {S, P}, "S2.4: Insert S at Pos=0");

    // --- S2.5: Re-insert S at Pos=0 → not in [0,0), so reinsert (no change) ===
    insert_unique_compact<0>(small2, S);
    expect_array_equals(small2, {S, P}, "S2.5: Re-insert S");
}

// -----------------------------------------------------------------------------
// ✅ Test: insert_unique_pos — always inserts/moves to Pos
// -----------------------------------------------------------------------------
void test_insert_unique_pos() {
    std::array<HistoryMove, 3> posArr = {};
    auto clearPos = [&]() { std::memset(posArr.data(), 0, sizeof(posArr)); };

    auto X = randomMove();
    auto Y = randomMove();
    auto Z = randomMove();

    // === Test P1: Insert X at Pos=1 → goes to index 1 ===
    clearPos();
    insert_unique_pos<1>(posArr, X);
    expect_array_equals(posArr, {HistoryMove{}, X, HistoryMove{}}, "P1: Insert X at Pos=1");

    // === Test P2: Insert Y at Pos=1 → shift right → Y@1, X@2 ===
    insert_unique_pos<1>(posArr, Y);
    expect_array_equals(posArr, {HistoryMove{}, Y, X}, "P2: Insert Y at Pos=1");

    // === Test P3: Re-insert X → found at index 2 ≥ Pos=1 → move to Pos=1, shift Y→2 ===
    insert_unique_pos<1>(posArr, X);
    expect_array_equals(posArr, {HistoryMove{}, X, Y}, "P3: Re-insert X");

    // === Test P4: Re-insert X again → stable ===
    insert_unique_pos<1>(posArr, X);
    expect_array_equals(posArr, {HistoryMove{}, X, Y}, "P4: Re-insert X again");

    // === Test P5: Insert Z at Pos=0 → insert at front ===
    insert_unique_pos<0>(posArr, Z);
    expect_array_equals(posArr, {Z, X, Y}, "P5: Insert Z at Pos=0");


    // === Test T1: Array size = 1 ===
    std::array<HistoryMove, 1> small1{};
    auto clear1 = [&]() { std::memset(small1.data(), 0, sizeof(small1)); };

    auto A = randomMove();
    auto B = randomMove();

    clear1();
    insert_unique_pos<0>(small1, A);
    expect_array_equals(small1, {A}, "T1.1: N=1, insert A at Pos=0");

    insert_unique_pos<0>(small1, A);
    expect_array_equals(small1, {A}, "T1.2: N=1, re-insert A");

    insert_unique_pos<0>(small1, B);
    expect_array_equals(small1, {B}, "T1.3: N=1, insert B → evicts A");


    // === Test T2: Array size = 2 ===
    std::array<HistoryMove, 2> small2{};
    auto clear2 = [&]() { std::memset(small2.data(), 0, sizeof(small2)); };

    auto M1 = randomMove();
    auto M2 = randomMove();
    auto M3 = randomMove();

    clear2();
    insert_unique_pos<0>(small2, M1);
    expect_array_equals(small2, {M1, {}}, "T2.1: N=2, insert M1 at Pos=0");

    insert_unique_pos<1>(small2, M2);
    expect_array_equals(small2, {M1, M2}, "T2.2: N=2, insert M2 at Pos=1");

    insert_unique_pos<0>(small2, M3);
    expect_array_equals(small2, {M3, M1}, "T2.3: N=2, insert M3 at Pos=0 → shifts M1 right, M2 lost");

    insert_unique_pos<0>(small2, M1);
    expect_array_equals(small2, {M1, M3}, "T2.4: N=2, re-insert M1 → moves to front");


    // === Test T3: Size=4, insert at Pos=0 and Pos=2 ===
    std::array<HistoryMove, 4> bigArr = {};
    auto clearBig = [&]() { std::memset(bigArr.data(), 0, sizeof(bigArr)); };

    auto V1 = randomMove();
    auto V2 = randomMove();
    auto V3 = randomMove();
    auto V4 = randomMove();

    clearBig();
    insert_unique_pos<0>(bigArr, V1);
    expect_array_equals(bigArr, {V1, {}, {}, {}}, "T3.1: N=4, insert V1 at Pos=0");

    insert_unique_pos<0>(bigArr, V2);
    expect_array_equals(bigArr, {V2, V1, {}, {}}, "T3.2: N=4, insert V2 at Pos=0");

    insert_unique_pos<2>(bigArr, V3);
    expect_array_equals(bigArr, {V2, V1, V3, {}}, "T3.3: N=4, insert V3 at Pos=2");

    insert_unique_pos<2>(bigArr, V4);
    expect_array_equals(bigArr, {V2, V1, V4, V3}, "T3.4: N=4, insert V4 at Pos=2 → shifts V3 to 3");

    insert_unique_pos<2>(bigArr, V1);
    expect_array_equals(bigArr, {V2, V1, V4, V3}, "T3.5: N=4, re-insert V1 at Pos=2 → already at lower index → no change");
}

// -----------------------------------------------------------------------------
// ✅ Test Namespace
// -----------------------------------------------------------------------------
namespace TestHistoryMoves {
    void test() {
        test_history_moves();
        test_insert_unique_compact();
        test_insert_unique_pos();
    }
}
