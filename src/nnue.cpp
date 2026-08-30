#include "nnue.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 6
#include "incbin.h"

struct CACHE_ALIGN IncbinNnue {
    Nnue::W0 w0; // feature weights, feature biases embeded into both sides kings weights
    array<Nnue::_t, Nnue::DualAccIndex> w1; // output weights
    i64_t b1; // output bias, padded to 64 bytes
};

INCBIN(IncbinNnue, incbin_nnue, "net/quantised.bin");

Nnue::Nnue() {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }

    const IncbinNnue& incbin_nnue = *incbin_nnue_data;

    w0 = incbin_nnue.w0; // copy as is
    b1 = incbin_nnue.b1; // copy as is

    for (auto n : range<DualAccIndex>()) {
        auto w = incbin_nnue.w1[n];
        for (int lane = 0; lane < 16; ++lane) {
            // 1) any rounding happens only when _w_ lowest bit is one
            // 2) _mm256_mulhrs_epi16 rounds positive product up, negative -- towards zero
            // 3) _mm256_madd_epi16 adds even and odd lanes together
            // 4) compensate systematic upward error by rounding down odd _w_ on odd lane
            if ((w[lane] & 1) && (lane & 1)) {
                w[lane] -= 1;
            }
        }
        w1[n] = w;
    }

    #ifndef NDEBUG
        i16_t w_max = 0;
        for (auto f : range<FeatureIndex>() ){
            for (auto n : range<AccIndex>()) {
                auto w = w0[f][n];

                for (int lane = 0; lane < 16; ++lane) {
                    if (w_max < std::abs(w[lane])) { w_max = std::abs(w[lane]); }
                }
            }
        }
        std::cout << "w0 max: " << w_max << std::endl;

        w_max = 0;
        for (auto n : range<DualAccIndex>()) {
            auto w = w1[n];
            for (int lane = 0; lane < 16; ++lane) {
                if (w_max < std::abs(w[lane])) { w_max = std::abs(w[lane]); }
            }
        }
        std::cout << "w1 max: " << w_max << std::endl;
    #endif
}
