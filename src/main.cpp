#include <sysexits.h>
#include "assert.hpp"
#include "io.hpp"
#include "nnue.hpp"
#include "Bb.hpp"
#include "Evaluation.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Uci.hpp"
#include "VectorOfAll.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

// global const default nnue value
INCBIN(EmbeddedNnue, "net/petrel128.bin");

/**
* Startup constant initialization
*/
const InBetween inBetween; // 32k 64*64*8
const HyperbolaDir hyperbolaDir; // 4k 64*4*16
const HyperbolaSq hyperbolaSq; // 1k 64*16
const AttacksFrom attacksFrom; // 3k 6*64*8
constexpr const VectorOfAll vectorOfAll; // 4k 256*16, used by bitReverse initialization
const PieceSquareTable pieceSquareTable; // 3k 6*64*8
constexpr const PiSingle piSingle; // 256
const CastlingRules castlingRules; // 128
constexpr const BitReverse bitReverse; // 32

// global Uci instance
Uci The_uci(std::cout);

// global almost constant instance
Nnue nnue;

bool Nnue::setEmbeddedEval() {
    if (EmbeddedNnueSize != sizeof(Nnue)) { return false; }
    std::memcpy(this, EmbeddedNnueData, sizeof(Nnue));
    return true;
}

void io::log(const std::string& message) {
    The_uci.log(message);
}

int main(int argc, const char* argv[]) {
    std::string initFileName;

    for (int i = 1; i < argc; ++i) {
        std::string option = argv[i];

        if (option == "--version" || option == "-v") {
            std::cout
                << io::app_version << '\n'
                << "(c) Aleks Peshkov (aleks.peshkov@gmail.com)\n"
            ;
            return EXIT_SUCCESS;
        }

        if (option == "--help" || option == "-h") {
            std::cout
                << "    Petrel: UCI chess engine\n\n"
                << "Options:\n"
                << "      -h, --help        display this help\n"
                << "      -v, --version     display version information\n"
                << "      -f, --file [FILE] read file for initial UCI commands\n"
            ;
            return EXIT_SUCCESS;
        }

        if (option == "--file" || option == "-f") {
            if (i < argc) {
                initFileName = argv[++i];
                break;
            }
        }

        std::cerr << "petrel: unknown option\n";
        return EINVAL;
    }

    if (!nnue.setEmbeddedEval()) {
        std::cerr << "petrel: fatal error: embedded NNUE data file has invalid size, expected " << sizeof(Nnue) << " bytes, \n";
        return EX_SOFTWARE;
    }

    // speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    The_uci.ucinewgame();

    if (!initFileName.empty()) {
        std::ifstream initFile{initFileName};
        if (initFile) {
            The_uci.processInput(initFile);
        } else {
            std::cerr << "petrel: error opening file: " << initFileName << '\n';
            return EINVAL;
        }
    }

    The_uci.processInput(std::cin);

    return EXIT_SUCCESS;
}
