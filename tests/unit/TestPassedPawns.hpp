#define NDEBUG 1

#include "Hyperbola.hpp"
#include "UciPosition.hpp"

Nnue nnue;
constexpr InBetween inBetween;
constexpr AttacksFrom attacksFrom;
constexpr VectorOfAll vectorOfAll;
constexpr PiSingle piSingle;
constexpr CastlingRules castlingRules;
constexpr PieceCountTable pieceCountTable;

void assertPassed(const char* fen, Square::_t sq, bool shouldBePassed, const char* msg) {
    UciPosition uciPosition;
    std::istringstream is{fen};
    uciPosition.readFen(is);
    const Position& pos = uciPosition;

    Square square{sq};
    if (!pos.MY.has(square)) {
        std::cerr << "ERROR: No pawn at " << square << " in FEN: " << fen << "\n";
        assert(false);
    }

    bool isPassed = pos.MY.bbPassedPawns().has(square);
    if (isPassed != shouldBePassed) {
        std::cerr << "FAIL: " << msg
                  << " -> expected " << (shouldBePassed ? "true" : "false")
                  << ", got " << (isPassed ? "true" : "false")
                  << " [FEN: " << fen << "]\n";
        assert(false);
    }
}

void test_fen_passed_pawn() {
    // === Basic passed pawns ===
    assertPassed("8/8/4P1k1/8/8/8/7K/8 w - - 0 1", E6, true, "E6 passed");
    assertPassed("8/5p1k/4P3/8/8/8/7K/8 w - - 0 1", E6, true, "E6 passed F7");
    assertPassed("8/8/8/4p1k1/4P3/8/7K/8 w - - 0 1", E4, false, "E4 blocked E5");
    assertPassed("8/8/8/3p2k1/8/4P3/7K/8 w - - 0 1", E3, false, "E3 blocked D5");
    assertPassed("8/8/8/8/8/3pP3/5k1K/8 w - - 0 1", E3, true, "E3 passed D3");

    // === A-file ===
    assertPassed("2k5/8/8/8/8/1p6/P7/7K w - - 0 1", A2, true, "A2 passed: B3 attacks A2 only");
    assertPassed("2k5/8/8/1p6/8/8/P7/7K w - - 0 1", A2, false, "A2 blocked B4 attack on A3");
    assertPassed("2k5/8/8/8/8/8/Pp5K/8 w - - 0 1", A2, true, "A2 passed B2");

    // === H-file ===
    assertPassed("2k5/6pP/8/8/8/8/7K/8 w - - 0 1", H7, true, "H7 passed G7");

    // === A7 and A3 ===
    assertPassed("2k5/P7/8/8/8/8/8/3K4 w - - 0 1", A7, true, "A7 passed");
    assertPassed("2k5/8/8/1p6/8/P7/8/3K4 w - - 0 1", A3, false, "A3 blocked B5 attack on A4");

    // === Doubled pawns ===
    assertPassed("2k5/8/8/8/8/P7/P7/3K4 w - - 0 1", A2, true, "A2 passed doubled A3");
    assertPassed("2k5/8/8/8/8/7P/7P/3K4 w - - 0 1", H2, true, "H2 passed doubled H3");
    assertPassed("2k5/8/8/6p1/8/7P/7P/3K4 w - - 0 1", H2, false, "H2 blocked G4 attack on H3");

    // === Complex ===
    assertPassed("2k5/8/8/8/3p4/8/2P5/3K4 w - - 0 1", C2, false, "C2 blocked D4 attack on C3");
    assertPassed("2k5/8/3p1p2/8/4P3/8/8/3K4 w - - 0 1", E4, false, "E4 blocked D5/F5 attack on E6");
    assertPassed("2k4K/8/6p1/7P/8/8/8/8 w - - 0 1", H5, true, "H5 passed: G6 attacks H5 only");
    assertPassed("2k5/8/8/6p1/7P/8/8/3K4 w - - 0 1", H4, true, "H4 passed: G5 attacks H4 only");
    assertPassed("2k5/8/6p1/8/7P/8/8/3K4 w - - 0 1", H4, false, "H4 blocked G6 attack on H5");
}

namespace TestPassedPawns {
    void test() {
        test_fen_passed_pawn();
    }
}
