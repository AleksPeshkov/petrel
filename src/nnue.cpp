#include "nnue.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 6
#include "incbin.h"

struct CACHE_ALIGN IncbinNnue {
    Nnue::W0 w0; // feature weights, feature biases embeded into both sides kings weights
    array<Nnue::_t, Nnue::ConcatIndex, Side, Nnue::AccIndex> w1; // output weights
    i64_t b1; // output bias, padded to 64 bytes
};

INCBIN(IncbinNnue, incbin_nnue, "net/quantised.bin");

Nnue::Nnue() {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }

    const IncbinNnue& incbin = *incbin_nnue_data;
    w0 = incbin.w0; // copy as is

    for (auto side : range<Side>()) {
        for (auto n : range<AccIndex>()) {
            w1[side][n][Pos] = incbin.w1[Pos][side][n];
            w1[side][n][Neg] = incbin.w1[Neg][side][n];
        }
    }

    b1 = incbin.b1; // copy as is

    #ifndef NDEBUG
        i16_t w_max = 0;
        for (auto f : range<Fi>() ){
            for (auto n : range<AccIndex>()) {
                auto w = w0[f][n];
                for (int lane = 0; lane < 16; ++lane) {
                    if (w_max < std::abs(w[lane])) { w_max = std::abs(w[lane]); }
                }
            }
        }
        std::cout << "w0 max: " << w_max << std::endl;

        w_max = 0;
        auto w_min = 32768;
        for (auto side : range<Side>()) {
            for (auto n : range<AccIndex>()) {
                for (int lane = 0; lane < 16; ++lane) {
                    auto x = std::max(std::abs(w1[side][n][Pos][lane]), std::abs(w1[side][n][Neg][lane]));
                    if (w_min > x) { w_min = x; }
                    if (w_max < x) { w_max = x; }
                }
            }
        }
        std::cout << "w1 min: " << w_min << std::endl;
        std::cout << "w1 max: " << w_max << std::endl;
    #endif
}
