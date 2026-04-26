#include "assert.hpp"
#include "TestPassedPawns.hpp"
#include "TestHyperbola.hpp"
#include "TestHistoryMoves.hpp"
#include "TestRepetitions.hpp"

/* mocks */

void io::error(std::string_view) {}
void io::info(std::string_view) {}

void assert_fail(const char* assertion, const char* file, unsigned int line, const char* func) {
    std::cerr << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")";
    std::exit(EXIT_FAILURE); // graceful exit without core dump
}

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
