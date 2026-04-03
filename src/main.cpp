#include "io.hpp"
#include "nnue.hpp"
#include "Bb.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Score.hpp"
#include "Uci.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

/**
* Startup constant initialization
*/
constexpr InBetween inBetween; // 32k 64*64*8, used by constexpr CastlingRules
constinit HyperbolaDir hyperbolaDir; // 4k 64*4*16
constinit HyperbolaSq hyperbolaSq; // 1k 64*16
constinit AttacksFrom attacksFrom; // 3k 6*64*8
constexpr VectorOfAll vectorOfAll; // 4k 256*16
constexpr PiSingle piSingle; // 256
constinit CastlingRules castlingRules; // 128
constinit PieceCountTable pieceCountTable; // 48 6*8

// global const default nnue value
INCBIN(EmbeddedNnue, "net/quantised.bin");

// global almost constant instance
Nnue nnue;

// copy NNUE weigths from embedded binary
void Nnue::setEmbeddedEval() {
    assert (EmbeddedNnueSize == sizeof(Nnue));
    std::memcpy(this, EmbeddedNnueData, sizeof(Nnue));
}

// global Uci instance
Uci The_uci(std::cout);

void io::error(std::string_view message) {
    The_uci.error(message);
}

void io::info(std::string_view message) {
    The_uci.info(message);
}

#ifndef NDEBUG
void assert_fail(const char* assertion, const char* file, unsigned int line, const char* func) {
    std::ostringstream os;
    os << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")";
    io::error(os.str());

    std::exit(EXIT_FAILURE); // graceful exit without core dump
    __builtin_unreachable();
}
#endif

int main(int argc, const char* argv[]) {
    if (EmbeddedNnueSize != sizeof(Nnue)) {
        std::cerr << "petrel: fatal error: embedded NNUE data file has invalid size, expected " << sizeof(Nnue) << " bytes, \n";
        return ENOEXEC;
    }

    std::string initFileName;
    bool runBench = false;
    std::string benchLimits;

    for (int i = 1; i < argc; ++i) {
        std::string_view option{argv[i]};

        if (option == "--file" || option == "-f") {
            if (++i >= argc) {
                std::cerr << "petrel: option '" << option << "' requires a filename\n";
                return EXIT_FAILURE;
            }

            initFileName = argv[i];
            continue;
        }

        if (option == "bench" || option == "--bench" || option == "-b") {
            // collect all remaining arguments
            while (++i < argc) {
                benchLimits += argv[i];
                benchLimits += " ";
            }

            runBench = true;
            continue;
        }

        if (option == "--version" || option == "-v") {
            std::cout << io::app_version << '\n';
            return EXIT_SUCCESS;
        }

        if (option == "--help" || option == "-h") {
            std::cout
                << "    Petrel: UCI chess engine\n"
                << "\nOptions:\n"
                << "    -f|--file [FILE]                Read and execute initial UCI commands from the specified file.\n"
                << "    -b|--bench|bench [GO LIMITS]    Search a set of benchmark positions, report total nodes and nps, and exit.\n"
                << "    -v|--version                    Display version information and exit.\n"
                << "    -h|--help                       Show this help message and exit.\n"
                << "\n";
            ;
            return EXIT_SUCCESS;
        }

        std::cerr << "petrel: unknown option " << option << '\n';
        return EXIT_FAILURE;
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

    if (runBench) {
        The_uci.bench(benchLimits);
        return EXIT_SUCCESS;
    }

    The_uci.processInput(std::cin);
    return EXIT_SUCCESS;
}
