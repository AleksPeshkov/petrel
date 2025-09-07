#include <errno.h>
#include "assert.hpp"
#include "io.hpp"
#include "Bb.hpp"
#include "Evaluation.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Uci.hpp"
#include "VectorOfAll.hpp"

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

template <> io::czstring PieceType::The_string{"qrbnpk"};
template <> io::czstring PromoType::The_string{"qrbn"};
template <> io::czstring Color::The_string{"wb"};
template <> io::czstring CastlingSide::The_string{"kq"};

// global pointer to Uci instance to implement io::log()
const Uci* The_uci = nullptr;

void io::log(const std::string& message) {
    if (The_uci) { The_uci->log(message); }
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
                << "    Petrel chess engine. The UCI protocol compatible.\n\n"
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

    // speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    Uci uci(std::cout);
    The_uci = &uci;

    if (!initFileName.empty()) {
        std::ifstream initFile{initFileName};
        if (initFile) {
            uci.processInput(initFile);
        } else {
            std::cerr << "petrel: error opening file: " << initFileName << '\n';
            return EINVAL;
        }
    }

    uci.processInput(std::cin);

    return EXIT_SUCCESS;
}
