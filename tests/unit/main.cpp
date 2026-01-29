#include "TestPassedPawns.hpp"
#include "TestHyperbola.hpp"
#include "TestHistoryMoves.hpp"
#include "TestRepetitions.hpp"

/* mocks */

void Nnue::setEmbeddedEval() {}
void io::log(std::string_view) {}

int main() {
    try {
        TestPassedPawns::test();
        TestHyperbola::test();
        TestHistoryMoves::test();
        TestRepetitions::test();

        std::cerr << "✅ All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
        return 1;
    }
}
