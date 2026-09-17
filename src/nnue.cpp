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
