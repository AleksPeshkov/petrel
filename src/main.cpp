#include <errno.h>
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
constexpr HyperbolaDir hyperbolaDir; // 4k 64*4*16
constexpr HyperbolaSq hyperbolaSq; // 1k 64*16
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

void Nnue::setEmbeddedEval() {
    assert (EmbeddedNnueSize == sizeof(Nnue));
    std::memcpy(this, EmbeddedNnueData, sizeof(Nnue));
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
                << "      -h, --help               Show this help message and exit.\n"
                << "      -v, --version            Display version information and exit.\n"
                << "      -f [FILE], --file [FILE] Read and execute UCI commands from the specified file\n"
            ;
            return EXIT_SUCCESS;
        }

        if (option == "--file" || option == "-f") {
            if (++i >= argc) {
                std::cerr << "petrel: option '" << option << "' requires a filename\n";
                return EXIT_FAILURE;
            }

            initFileName = argv[i];
            continue;
        }

        std::cerr << "petrel: unknown option\n";
        return EXIT_FAILURE;
    }

    if (EmbeddedNnueSize != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: embedded NNUE data file has invalid size, expected " << sizeof(Nnue) << " bytes, \n";
        return ENOEXEC;
    }

    // speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    if (!initFileName.empty()) {
        std::ifstream initFile{initFileName};
        if (!initFile) {
            std::cerr << "petrel: error opening config file: " << initFileName << '\n';
            return EXIT_FAILURE;
        }

        The_uci.processInput(initFile);
    }

    The_uci.processInput(std::cin);
    return EXIT_SUCCESS;
}
