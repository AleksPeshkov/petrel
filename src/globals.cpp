#include "common.hpp"
#include "io.hpp"
#include "nnue.hpp"
#include "Bb.hpp"
#include "Hyperbola.hpp"
#include "PiMask.hpp"
#include "Score.hpp"
#include "Tt.hpp"
#include "Uci.hpp"

const Nnue nnue;
constexpr const InBetween inBetween; // 32k 64*64*8, used by constexpr CastlingRules
constinit const HyperbolaDir hyperbolaDir; // 4k 64*4*16
constinit const HyperbolaSq hyperbolaSq; // 1k 64*16
constinit const AttacksFrom attacksFrom; // 3k 6*64*8
constinit const PiOneMask piOneMask; // 256
constinit const CastlingRules castlingRules; // 128
constinit const PieceCountTable pieceCountTable; // 48 6*8

// global TT instance
Tt the_tt{64 * 1024 * 1024};

// global Uci instance
Uci the_uci{std::cout};

void io::error(std::string_view message) {
    the_uci.error(message);
}

#ifndef NDEBUG
void assert_fail(const char* assertion, const char* file, unsigned int line, const char* func) {
    io::error( std::string("Assertion failed (") + file + ":" + std::to_string(line) + "): " + func + ": " + assertion);
    std::exit(EXIT_FAILURE); // graceful exit without core dump
    __builtin_unreachable();
}
#endif
