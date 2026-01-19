#include "Hyperbola.hpp"

constexpr HyperbolaDir hyperbolaDir; // 4k 64*2*32
constexpr HyperbolaSq hyperbolaSq; // 2k 64*32

void test_hyperbola_rook_attack() {
    Square from{D4};
    Bb occupied = Bb{D1} + Bb{D4} + Bb{D7};  // includes slider

    Hyperbola h(occupied);
    Bb attacks = h.attack(SliderType{Rook}, from);

    Bb expected =
        // File
        Bb{D1} + Bb{D2} + Bb{D3} + Bb{D5} + Bb{D6} + Bb{D7} +
        // Rank
        Bb{A4} + Bb{B4} + Bb{C4} + Bb{E4} + Bb{F4} + Bb{G4} + Bb{H4};

    if (attacks != expected) {
        std::cerr << "Occupied:\n" << occupied << "\n";
        std::cerr << "Attacks:\n" << attacks << "\n";
        std::cerr << "Expected:\n" << expected << "\n";
    }

    assert((attacks == expected) && "Rook attack failed");
}

void test_hyperbola_bishop_attack() {
    Square from{E4};
    Bb occupied = Bb{E4} + Bb{C2} + Bb{H7};
    Hyperbola h(occupied);

    Bb attacks = h.attack(SliderType{Bishop}, from);

    Bb expected =
        // A1-H8 diagonal
        Bb{D3} + Bb{C2} + Bb{F5} + Bb{G6} + Bb{H7} +
        // A8-H1 diagonal (full ray, no blockers)
        Bb{D5} + Bb{C6} + Bb{B7} + Bb{A8} +
        Bb{F3} + Bb{G2} + Bb{H1};

    if (attacks != expected) {
        std::cerr << "Occupied:\n" << occupied << "\n";
        std::cerr << "Attacks:\n" << attacks << "\n";
        std::cerr << "Expected:\n" << expected << "\n";
    }

    assert((attacks == expected) && "Bishop attack failed on diagonal");
}

void test_hyperbola_queen_attack() {
    Square from{E4};
    Bb occupied = Bb{E4} + Bb{E1} + Bb{E7} + Bb{C2} + Bb{G6};
    Hyperbola h(occupied);

    Bb attacks = h.attack(SliderType{Queen}, from);

    Bb expected =
        // Rook: E-file
        Bb{E1} + Bb{E2} + Bb{E3} + Bb{E5} + Bb{E6} + Bb{E7} +
        // Rook: E-rank
        Bb{A4} + Bb{B4} + Bb{C4} + Bb{D4} + Bb{F4} + Bb{G4} + Bb{H4} +
        // Bishop: A1-H8 diagonal
        Bb{D3} + Bb{C2} + Bb{F5} + Bb{G6} +
        // Bishop: A8-H1 diagonal
        Bb{D5} + Bb{C6} + Bb{B7} + Bb{A8} +
        Bb{F3} + Bb{G2} + Bb{H1};

    if (attacks != expected) {
        std::cerr << "Occupied:\n" << occupied << "\n";
        std::cerr << "Attacks:\n" << attacks << "\n";
        std::cerr << "Expected:\n" << expected << "\n";
    }

    assert((attacks == expected) && "Queen attack failed");
}

namespace TestHyperbola {
    void test() {
        test_hyperbola_rook_attack();
        test_hyperbola_bishop_attack();
        test_hyperbola_queen_attack();
    }
}
