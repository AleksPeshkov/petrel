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

void io::error(std::string_view message) {
    The_uci.error(message);
}

void io::info(std::string_view message) {
    The_uci.info(message);
}

#ifdef ENABLE_ASSERT_LOGGING
void assert_fail(const char *assertion, const char *file, unsigned int line, const char* func) {
    std::ostringstream oss;
    oss << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")\n";
    oss << "node " << The_uci.limits.getNodes() << " root fen " << The_uci.position_ << '\n';
    if (!The_uci.debugPosition.empty()) { oss << The_uci.debugPosition << '\n'; }
    if (!The_uci.debugGo.empty()) { oss << The_uci.debugGo << '\n'; }

    std::string message = oss.str();
    The_uci.error(message);

    std::exit(EXIT_FAILURE); // graceful exit without core dump
    __builtin_unreachable();
}
#endif

int main(int argc, const char* argv[]) {
    if (argc > 1) {
        std::string option = argv[1];

        if (option == "--version" || option == "-v") {
            std::cout
                << io::app_version << '\n'
                << "(c) Aleks Peshkov (aleks.peshkov@gmail.com)\n"
            ;
            return EXIT_SUCCESS;
        }

        if (option == "--help" || option == "-h") {
            std::cout
                << "    Petrel chess engine. The UCI protocol compatible.\n\n"
                << "      -h, --help        display this help\n"
                << "      -v, --version     display version information\n"
            ;
            return EXIT_SUCCESS;
        }

        std::cerr << "petrel: unknown option\n";
        return EXIT_FAILURE;
    }

    // speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    The_uci.processInput(std::cin);
    return EXIT_SUCCESS;
}
