#include "nnue.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 6
#include "incbin.h"

template <typename V>
constexpr V abs(V v) {
    #ifdef __clang__
        return __builtin_elementwise_abs(v);
    #else
        return v < 0 ? -v : v;
    #endif
}

struct CACHE_ALIGN IncbinNnue {
    Nnue::W0 w0; // feature weights, feature biases embeded into both sides kings weights
    Nnue::W1 w1; // output weights
    Nnue::B1 b1; // output bias, padded to 64 bytes
};

INCBIN(IncbinNnue, incbin_nnue, "net/quantised.bin");

Nnue::Nnue() {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }

    const IncbinNnue& incbin = *incbin_nnue_data;
    w0 = incbin.w0; // copy as is
    b1 = incbin.b1; // copy as is

    for (auto side : range<Side>()) {
        for (auto n : range<AccIndex>()) {
            auto w = incbin.w1[side][n];
            for (int lane = 0; lane < 16; ++lane) {
                // 1) rounding happens only when _w_ lowest bit is one
                // 2) _mm256_mulhrs_epi16 rounds positive product up, negative -- towards zero
                // 3) _mm256_madd_epi16 adds even and odd lanes together
                // 4) compensate systematic upward error by rounding down odd _w_ on odd lane
                if ((w[lane] & 1) && (lane & 1)) { w[lane] -= 1; }
            }
            w1[side][n] = w;
        }
    }

    #ifndef NDEBUG
        i16_t w_max = 0;
        for (auto f : range<Fi>() ){
            for (auto n : range<AccIndex>()) {
                auto w = abs(w0[f][n]);
                for (int lane = 0; lane < 16; ++lane) {
                    if (w_max < w[lane]) { w_max = w[lane]; }
                }
            }
        }
        std::cout << "w0 max: " << w_max << std::endl;

        w_max = 0;
        auto w_min = 32768;
        for (auto n : range<AccIndex>()) {
            auto w = max(abs(w1[My][n]), abs(w1[Op][n]));
            for (int lane = 0; lane < 16; ++lane) {
                if (w_min > w[lane]) { w_min = w[lane]; }
                if (w_max < w[lane]) { w_max = w[lane]; }
            }
        }
        std::cout << "w1 min: " << w_min << std::endl;
        std::cout << "w1 max: " << w_max << std::endl;
    #endif
}
