#include "common.hpp"
#include "io.hpp"
#include "nnue.hpp"
#include "Bb.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Score.hpp"
#include "Uci.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "incbin.h"

/**
* Startup constant initialization
*/
constexpr const InBetween inBetween; // 32k 64*64*8, used by constexpr CastlingRules
constinit const HyperbolaDir hyperbolaDir; // 4k 64*4*16
constinit const HyperbolaSq hyperbolaSq; // 1k 64*16
constinit const AttacksFrom attacksFrom; // 3k 6*64*8
constinit const PiOneMask piOneMask; // 256
constinit const CastlingRules castlingRules; // 128
constinit const PieceCountTable pieceCountTable; // 48 6*8

// global const default nnue value
INCBIN(incbin_nnue, "net/quantised.bin");

// global almost constant instance
Nnue nnue;

// copy NNUE weigths from embedded binary
void Nnue::setEmbeddedEval() {
    assert (incbin_nnue_size == sizeof(Nnue));
    std::memcpy(this, incbin_nnue_data, sizeof(Nnue));
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
    io::error( std::string("Assertion failed (") + file + ":" + std::to_string(line) + "): " + func + ": " + assertion);
    std::exit(EXIT_FAILURE); // graceful exit without core dump
    __builtin_unreachable();
}
#endif

ostream& io::app_version(ostream& os) {
    os << "petrel";

#ifdef VERSION
        os << ' ' << VERSION;
#endif

#ifdef GIT_DATE
    os << ' ' << GIT_DATE;
#else
    char year[] {__DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], '\0'};

    char month[] {
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '1' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '1' : '0',

        (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') ? '1' :
        (__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b') ? '2' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') ? '3' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r') ? '4' :
        (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') ? '5' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') ? '6' :
        (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') ? '7' :
        (__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g') ? '8' :
        (__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p') ? '9' :
        (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? '0' :
        (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? '1' :
        (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') ? '2' : '0',

        '\0'
    };

    char day[] {((__DATE__[4] == ' ') ? '0' : __DATE__[4]), __DATE__[5], '\0'};

    os << ' ' << year << '-' << month << '-' << day;
#endif

#ifdef GIT_ORIGIN
        os << ' ' << GIT_ORIGIN;
#endif

#ifdef GIT_SHA
        os << ' ' << GIT_SHA;
#endif

#ifndef NDEBUG
        os << " DEBUG";
#endif

    return os;
}

int main(int argc, const char* argv[]) {
    if (incbin_nnue_size != sizeof(Nnue)) {
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
