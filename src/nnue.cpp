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
        const lane_t W0_top1 = 1024;
        const lane_t W0_bottom1 = -1024;
        const lane_t W0_top2 = 2048;
        const lane_t W0_bottom2 = -2048;
        int w0_over_top1 = 0;
        int w0_under_bottom1 = 0;
        int w0_over_top2 = 0;
        int w0_under_bottom2 = 0;

        lane_t w0_max = -32768;
        lane_t w0_min = 32767;
        i64_t w0x_sum = 0;
        i64_t w0y_sum = 0;

        for (auto f : range<Fi>()) {
            for (auto n : range<AccIndex>()) {
                auto wx = w0[f][n][UnitX];
                auto wy = w0[f][n][UnitY];
                for (int lane = 0; lane < 16; ++lane) {
                    w0x_sum += std::abs(wx[lane]);
                    w0y_sum += std::abs(wy[lane]);
                }

                for (auto u : range<UnitIndex>()) {
                    auto w = w0[f][n][u];
                    for (int lane = 0; lane < 16; ++lane) {
                        if (w0_min > w[lane]) { w0_min = w[lane]; }
                        if (w0_max < w[lane]) { w0_max = w[lane]; }
                        if (w[lane] >= W0_top1) { ++w0_over_top1; }
                        if (w[lane] <= W0_bottom1) { ++w0_under_bottom1; }
                        if (w[lane] >= W0_top2) { ++w0_over_top2; }
                        if (w[lane] <= W0_bottom2) { ++w0_under_bottom2; }
                    }
                }
            }
        }
        std::cout << "ave |w0| = " << (w0x_sum + w0y_sum + W0_count/2) / W0_count << '\n';
        std::cout << " ave |w0x| = " << (w0x_sum + W0_count/4) / (W0_count/2) << '\n';
        std::cout << " ave |w0y| = " << (w0y_sum + W0_count/4) / (W0_count/2) << '\n';
        std::cout << " min w0 = " << w0_min << '\n';
        std::cout << "  <= " << W0_bottom1 << ": " << w0_under_bottom1 << " out of " << W0_count << '\n';
        std::cout << "  <= " << W0_bottom2 << ": " << w0_under_bottom2 << " out of " << W0_count << '\n';
        std::cout << " max w0 = " << w0_max << '\n';
        std::cout << "  >= " << W0_top1 << ": " << w0_over_top1 << " out of " << W0_count << '\n';
        std::cout << "  >= " << W0_top2 << ": " << w0_over_top2 << " out of " << W0_count << '\n';
        std::cout << std::endl;

        constexpr auto W1_count = sizeof(W1) / sizeof(i16_t);
        lane_t w1_min = 32767;
        lane_t w1a_min = 32767;
        lane_t w1b_min = 32767;
        lane_t w1_max = -32768;
        int w1_over_16k = 0;
        i64_t w1a_sum = 0;
        i64_t w1b_sum = 0;

        for (auto n : range<AccIndex>()) {
            auto wam = abs(w1[My][n][UnitX]);
            auto wao = abs(w1[Op][n][UnitX]);
            auto wbm = abs(w1[My][n][UnitY]);
            auto wbo = abs(w1[Op][n][UnitY]);
            auto wa = max(wam, wao);
            auto wb = max(wbm, wbo);
            auto w = max(wa, wb);

            for (int lane = 0; lane < 16; ++lane) {
                if (w1_max < w[lane]) { w1_max = w[lane]; }
                if (w1_min > w[lane]) { w1_min = w[lane]; }
                if (w1a_min > wa[lane]) { w1a_min = wa[lane]; }
                if (w1b_min > wb[lane]) { w1b_min = wb[lane]; }

                if (wam[lane] > 16383) { ++w1_over_16k; }
                if (wao[lane] > 16383) { ++w1_over_16k; }
                if (wbm[lane] > 16383) { ++w1_over_16k; }
                if (wbo[lane] > 16383) { ++w1_over_16k; }

                w1a_sum += wam[lane] + wao[lane];
                w1b_sum += wbm[lane] + wbo[lane];
            }
        }

        std::cout << "max |w1| = " << w1_max << "\n";
        std::cout << " >= 16384: " << w1_over_16k << " out of " << W1_count << '\n';
        std::cout << " ave |w1| = " << (w1a_sum + w1b_sum + W1_count/2) / W1_count << '\n';
        std::cout << "  ave |w1a| = " << (w1a_sum + W1_count/4) / (W1_count/2) << '\n';
        std::cout << "  ave |w1b| = " << (w1b_sum + W1_count/4) / (W1_count/2) << '\n';
        std::cout << "min max(w1) = " << w1_min << '\n';
        std::cout << " min max(w1a) = " << w1a_min << '\n';
        std::cout << " min max(w1b) = " << w1b_min << '\n';
        std::cout << std::endl;
    #endif
}
