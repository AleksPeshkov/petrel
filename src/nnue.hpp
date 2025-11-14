#ifndef NNUE_HPP
#define NNUE_HPP

#include "typedefs.hpp"

using vi16x16_t = i16_t __attribute__((vector_size(32)));

// Made compatible with net files made for
// Publius chess engine by Pawel Koziol

// NNUE evaluation. Net architecture and constants make it
// equivalent to the simple example provided by the bullet trainer:
// https://github.com/jw1912/bullet/blob/main/examples/simple.rs
// The architecture is (768 -> HIDDEN_SIZE)x2 -> 1

struct alignas(64) Nnue {
    static constexpr int HIDDEN_SIZE = 32;

    using IndexL0 = Index<768>;
    using IndexL1 = Index<HIDDEN_SIZE/16>;
    using IndexL2 = Index<2*IndexL1::Size>;

    IndexL0::arrayOf<IndexL1::arrayOf<vi16x16_t>> inputWeights;
    IndexL1::arrayOf<vi16x16_t> hiddenBiases;
    IndexL2::arrayOf<vi16x16_t> outputWeights;
    i16_t outputBias;
};

extern Nnue nnue;

#endif
