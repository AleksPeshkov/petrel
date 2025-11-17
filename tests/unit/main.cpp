#define TEST_ASSERT
#include "TestHistoryMoves.hpp"
#include "TestRepetitions.hpp"

void assert_fail(const char *assertion, const char *file, unsigned int line, const char* func) {
    std::ostringstream oss;
    oss << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")";
    std::string message = oss.str();
    //std::cerr << message << std::endl;

    throw std::runtime_error(message); // Throw for test harness
}

int main() {
    try {
        TestHistoryMoves::test();
        TestRepetitions::test();

        std::cerr << "✅ All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
        return 1;
    }
}
