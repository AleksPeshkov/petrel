#include <errno.h>

#include "io.hpp"
#include "AttacksFrom.hpp"
#include "BitReverse.hpp"
#include "CastlingRules.hpp"
#include "Evaluation.hpp"
#include "Hyperbola.hpp"
#include "PiOrder.hpp"
#include "PiSingle.hpp"
#include "SquaresInBetween.hpp"
#include "Uci.hpp"
#include "VectorOfAll.hpp"
#include "Zobrist.hpp"

/**
* Startup constant initialization
*/
const SquaresInBetween inBetween; //32k 64*64*8
const HyperbolaDir hyperbolaDir; //4k 64*4*16
const HyperbolaSq hyperbolaSq; //1k 64*16
const AttacksFrom attacksFrom; //3k 6*64*8
constexpr const VectorOfAll vectorOfAll; //4k 256*16
const PieceSquareTable pieceSquareTable; //3k 6*64*8
constexpr const PiSingle piSingle; //256
const CastlingRules castlingRules; //128
constexpr const ZobristKey zobristKey; //64 8*8
constexpr const BitReverse bitReverse; //32
constexpr const VectorBitCount bitCount; //16

template <> io::czstring PieceType::The_string{"qrbnpk"};
template <> io::czstring PromoType::The_string{"qrbn"};
template <> io::czstring Color::The_string{"wb"};
template <> io::czstring CastlingSide::The_string{"kq"};
template <> io::czstring Pi::The_string{"KQRrBbNn12345678"};

namespace {

using std::ostream;

ostream& app_version(ostream& out) {

#ifndef NDEBUG
    out << " DEBUG";
#endif

#ifdef GIT_DATE
    out << ' ' << GIT_DATE;
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

    out << ' ' << year << '-' << month << '-' << day;
#endif

#ifdef GIT_ORIGIN
    out << ' ' << GIT_ORIGIN;
#endif

#ifdef GIT_SHA
    out << ' ' << GIT_SHA;
#endif

    return out;
}

ostream& option_version(ostream& out) {
    return out
        << "petrel" << app_version << '\n'
        << "(c) Aleks Peshkov (aleks.peshkov@gmail.com)\n"
    ;
}

ostream& option_help(ostream& out) {
    return out
        << "    Petrel chess engine. The UCI protocol compatible.\n\n"
        << "      -h, --help        display this help\n"
        << "      -v, --version     display version information\n"
    ;
}

ostream& option_invalid(ostream& err) {
    return err << "petrel: unkown option\n";
}

} // anonymous namespace

int main(int argc, const char* argv[]) {
    if (argc > 1) {
        std::string option = argv[1];

        if (option == "--help" || option == "-h") {
            option_help(std::cout);
            return EXIT_SUCCESS;
        }

        if (option == "--version" || option == "-v") {
            option_version(std::cout);
            return EXIT_SUCCESS;
        }

        option_invalid(std::cerr);
        return EINVAL;
    }

    //speed tricks
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr.tie(nullptr);

    Uci uci(std::cout);
    uci(std::cin, std::cerr);

    //TRICK: simple "return 0;" does not work
    std::exit(EXIT_SUCCESS);
}
