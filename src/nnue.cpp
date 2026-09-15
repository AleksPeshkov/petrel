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

INCBIN(Nnue, incbin_nnue, "net/quantised.bin");
constinit const Nnue& nnue = *incbin_nnue_data;

void Nnue::validate_embedded() const {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }

    #ifndef NDEBUG
        i16_t w_max = -32767;
        i16_t w_min = 32767;
        for (auto f : range<Fi>() ) {
            for (auto n : range<AccIndex>()) {
                auto w = w0[f][n];
                for (int lane = 0; lane < 16; ++lane) {
                    if (w_min > w[lane]) { w_min = w[lane]; }
                    if (w_max < w[lane]) { w_max = w[lane]; }
                }
            }
        }
        std::cout << "w0 min: " << w_min << std::endl;
        std::cout << "w0 max: " << w_max << std::endl;

        w_max = -1;
        w_min = 32767;
        int w1_over_16k = 0;
        for (auto n : range<AccIndex>()) {
            auto w_my = abs(w1[My][n]);
            auto w_op = abs(w1[Op][n]);
            auto w = max(w_my, w_op);
            for (int lane = 0; lane < 16; ++lane) {
                if (w_min > w[lane]) { w_min = w[lane]; }
                if (w_max < w[lane]) { w_max = w[lane]; }
                if (w_my[lane] > 16383) { ++w1_over_16k; }
                if (w_op[lane] > 16383) { ++w1_over_16k; }
            }
        }
        std::cout << "|w1| pair min: " << w_min << std::endl;
        std::cout << "|w1| pair max: " << w_max << std::endl;
        std::cout << "|w1| > 16383: " << w1_over_16k << " out of " << Acc_neurons << std::endl;
    #endif
}
