#include <errno.h>

#include "io.hpp"
#include "Bb.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Score.hpp"
#include "Uci.hpp"

/**
* Startup constant initialization
*/
constexpr InBetween inBetween; // 32k 64*64*8, used by constexpr CastlingRules
constexpr HyperbolaDir hyperbolaDir; // 4k 64*4*16
constexpr HyperbolaSq hyperbolaSq; // 1k 64*16
constexpr AttacksFrom attacksFrom; // 3k 6*64*8
constexpr VectorOfAll vectorOfAll; // 4k 256*16
constexpr PieceSquareTable pieceSquareTable; // 3k 6*64*8
constexpr PiSingle piSingle; // 256
constexpr CastlingRules castlingRules; // 128

// global Uci instance
Uci The_uci(std::cout);

void io::log(std::string_view message) {
    The_uci.log(message);
}

#ifdef ENABLE_ASSERT_LOGGING
void assert_fail(const char *assertion, const char *file, unsigned int line, const char* func) {
    std::ostringstream oss;
    oss << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")\n";
    oss << "node " << The_uci.limits.getNodes() << " root fen " << The_uci.position_ << '\n';
    oss << The_uci.debugPosition << '\n';
    oss << The_uci.debugGo << '\n';

    std::string message = oss.str();
    io::log("#" + message);
    std::cerr << message << std::endl;

    std::exit(EXIT_FAILURE); // graceful exit without core dump
    __builtin_unreachable();
}
#endif

int main(int argc, const char* argv[]) {
    std::string initFileName;
    bool runBench = false;
    std::string benchLimits;

    for (int i = 1; i < argc; ++i) {
        std::string_view option{argv[i]};

        if (option == "--version" || option == "-v") {
            std::cout << io::app_version << '\n';
            return EXIT_SUCCESS;
        }

        if (option == "--help" || option == "-h") {
            std::cout
                << "    Petrel: UCI chess engine\n"
                << "\nOptions:\n"
                << "    -f [FILE], --file [FILE]    Read and execute initial UCI commands from the specified file.\n"
                << "    -h, --help                  Show this help message and exit.\n"
                << "    -v, --version               Display version information and exit.\n"
                << "\nOptional command:\n"
                << "    bench [GO LIMITS]           Search a set of test positions, report total nodes and nps, and exit.\n"
                << "\n";
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

        if (option == "bench" || option == "--bench" || option == "-b") {
            // collect all remaining arguments
            while (++i < argc) {
                benchLimits += argv[i];
                benchLimits += " ";
            }

            runBench = true;
            continue;
        }

        std::cerr << "petrel: unknown option\n";
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
