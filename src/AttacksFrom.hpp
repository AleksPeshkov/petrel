#ifndef ATTACKS_FROM_HPP
#define ATTACKS_FROM_HPP

#include "Bb.hpp"

//attack bitboards of the piece types on the empty board (3k)
class AttacksFrom {
    PieceType::arrayOf< Square::arrayOf<Bb> > attack;
public:
    constexpr AttacksFrom () {
        FOR_EACH (Square, sq) {
            attack[Rook][sq]   = sq.file() + sq.rank();
            attack[Bishop][sq] = sq.diagonal() + sq.antidiag();
            attack[Queen][sq]  = attack[Rook][sq] + attack[Bishop][sq];

            attack[Pawn][sq] = sq(-1, Rank3 - Rank2) + sq(+1, Rank3 - Rank2);

            attack[Knight][sq] =
                sq(+2, +1) + sq(+2, -1) + sq(+1, +2) + sq(+1, -2) +
                sq(-2, -1) + sq(-2, +1) + sq(-1, -2) + sq(-1, +2);

            attack[King][sq] =
                sq(+1, +1) + sq(+1, 0) + sq(0, +1) + sq(+1, -1) +
                sq(-1, -1) + sq(-1, 0) + sq(0, -1) + sq(-1, +1);
        }
    }

    constexpr const Bb& operator() (PieceType ty, Square sq) const { return attack[ty][sq]; }
};

extern const AttacksFrom attacksFrom;

#endif
