#include "TestRepetitions.hpp"

int main() {
    try {
        TestRepetitions::test();

        std::cerr << "✅ All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
        return 1;
    }
}
