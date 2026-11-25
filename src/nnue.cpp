#include "nnue.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 6
#include "incbin.h"

INCBIN(Nnue, incbin_nnue, "net/quantised.bin");
constinit const Nnue& nnue = *incbin_nnue_data;

void Nnue::validate_embedded() const {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }

    #ifndef NDEBUG
        using lane_t = i16_t;

        constexpr int W0_count = sizeof(W0) / sizeof(lane_t);
        const lane_t W0_top = 1536;
        const lane_t W0_bottom = -1536;
        int w0_at_top = 0;
        int w0_at_bottom = 0;

        lane_t w0_max = -32767;
        lane_t w0_min = 32767;
        i64_t w0_sum = 0;

        for (auto f : range<Fi>()) {
            for (auto n : range<AccIndex>()) {
                for (auto u : range<UnitIndex>()) {
                    auto w = w0[f][n][u];
                    for (int lane = 0; lane < 16; ++lane) {
                        if (w0_min > w[lane]) { w0_min = w[lane]; }
                        if (w0_max < w[lane]) { w0_max = w[lane]; }
                        if (w[lane] >= W0_top) { ++w0_at_top; }
                        if (w[lane] <= W0_bottom) { ++w0_at_bottom; }
                        w0_sum += std::abs(w[lane]);
                    }
                }
            }
        }
        std::cout << "min w0 = " << w0_min << ", <= " << W0_bottom << ": " << w0_at_bottom << " out of " << W0_count << std::endl;
        std::cout << "max w0 = " << w0_max << ", >= " << W0_top << ": " << w0_at_top << " out of " << W0_count << std::endl;
        std::cout << "ave |w0| = " << (w0_sum + W0_count/2) / W0_count << std::endl;

        constexpr auto W1_count = sizeof(W1) / sizeof(i16_t);
        int w1_min = 32767;
        int w1_max = -32767;
        int w1_over_16k = 0;
        i64_t w1_sum = 0;

        for (auto n : range<AccIndex>()) {
            auto wx_my = abs(w1[My][n][SignalX]);
            auto wy_my = abs(w1[My][n][SignalY]);
            auto wx_op = abs(w1[Op][n][SignalX]);
            auto wy_op = abs(w1[Op][n][SignalY]);

            auto w_unit = max(max(wx_my, wy_my), max(wx_op, wy_op));

            for (int lane = 0; lane < 16; ++lane) {
                if (w1_min > w_unit[lane]) { w1_min = w_unit[lane]; }
                if (w1_max < w_unit[lane]) { w1_max = w_unit[lane]; }

                if (wx_my[lane] > 16383) { ++w1_over_16k; }
                if (wy_my[lane] > 16383) { ++w1_over_16k; }
                if (wx_op[lane] > 16383) { ++w1_over_16k; }
                if (wy_op[lane] > 16383) { ++w1_over_16k; }

                w1_sum += wx_my[lane] + wy_my[lane] + wx_op[lane] + wy_op[lane];
            }
        }

        std::cout << "min max(|wx_my|,|wy_my|,|wx_op|,|wy_op|) = " << w1_min << std::endl;
        std::cout << "max |w1| = " << w1_max << ", > 16383: " << w1_over_16k << " out of " << W1_count << std::endl;
        std::cout << "ave |w1| = " << (w1_sum + W1_count / 2) / W1_count << std::endl;
    #endif
}
