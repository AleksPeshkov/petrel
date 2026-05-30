// tests/unit/TestTranspose.hpp

#include "bitops256.hpp"
#include <iostream>
#include <cassert>

void test_transpose() {
    struct CACHE_ALIGN test_t {
        u8_t  in[8][16];   // 8 rows × 16 bytes
        u64_t out[16];     // 16 outputs
    } test;

    // Fill input: in[row][col] = row * 16 + col
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 16; ++col) {
            test.in[row][col] = static_cast<u8_t>(row * 16 + col);
        }
    }

    // Call transpose (assumes reinterpretation works)
    transpose(
        reinterpret_cast<u64x4_t*>(test.out),
        reinterpret_cast<const u8x32_t*>(test.in)
    );

    // Verify all 16 outputs
    for (int col = 0; col < 16; ++col) {
        uint64_t expected = 0;
        for (int row = 0; row < 8; ++row) {
            u8_t byte = test.in[row][col]; // should be: row * 16 + col
            expected |= static_cast<uint64_t>(byte) << (8 * row);
        }

        if (test.out[col] != expected) {
            std::cerr << "Mismatch at out[" << col << "]:\n";
            std::cerr << "  Got     = 0x" << std::hex << std::setfill('0') << std::setw(16)
                      << test.out[col] << "\n";
            std::cerr << "  Expected = 0x" << std::setw(16) << expected << "\n";
            std::cerr << "  Col data: ";
            for (int row = 0; row < 8; ++row) {
                std::cerr << (row ? "," : "") << (int)test.in[row][col];
            }
            std::cerr << std::dec << "\n";

            assert(false && "Transpose failed");
        }
    }
}

namespace TestTranspose {
    void test() {
        test_transpose();
    }
}
