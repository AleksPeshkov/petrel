#include <errno.h>

#include "io.hpp"
#include "AttacksFrom.hpp"
#include "Bb.hpp"
#include "CastlingRules.hpp"
#include "Evaluation.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Uci.hpp"
#include "VectorOfAll.hpp"

/**
* Startup constant initialization
*/
const InBetween inBetween; //32k 64*64*8
const HyperbolaDir hyperbolaDir; //4k 64*4*16
const HyperbolaSq hyperbolaSq; //1k 64*16
const AttacksFrom attacksFrom; //3k 6*64*8
constexpr const VectorOfAll vectorOfAll; //4k 256*16, used by bitReverse initialization
const PieceSquareTable pieceSquareTable; //3k 6*64*8
constexpr const PiSingle piSingle; //256
const CastlingRules castlingRules; //128
constexpr const BitReverse bitReverse; //32

template <> io::czstring PieceType::The_string{"qrbnpk"};
template <> io::czstring PromoType::The_string{"qrbn"};
template <> io::czstring Color::The_string{"wb"};
template <> io::czstring CastlingSide::The_string{"kq"};
template <> io::czstring Pi::The_string{"KQRrBbNn12345678"};

// global pointer to Uci instance to implement io::log()
const Uci* The_uci = nullptr;

void io::log(const std::string& message) {
    if (The_uci) { The_uci->log(message); }
}

#ifdef ENABLE_ASSERT_LOGGING
void assert_fail(const char *assertion, const char *file, unsigned int line, const char* func) {
    std::ostringstream oss;
    oss << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")";
    std::string message = oss.str();
    io::log("#" + message);
    std::cerr << message << std::endl;

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
        return EINVAL;
    }

    // speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    Uci uci(std::cout);
    The_uci = &uci;

    uci.processInput(std::cin);

    return EXIT_SUCCESS;
}
