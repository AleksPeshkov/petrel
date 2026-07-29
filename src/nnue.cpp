#include "nnue.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 6
#include "incbin.h"

INCBIN(Nnue, incbin_nnue, "net/quantised.bin");

constinit const Nnue& nnue = *incbin_nnue_data;

void Nnue::validate_embedded_size() {
    if (incbin_nnue_size != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: invalid embedded NNUE file size: " << incbin_nnue_size << ", expected " << sizeof(Nnue) << " bytes\n";
        std::exit(EXIT_FAILURE);
    }
}
