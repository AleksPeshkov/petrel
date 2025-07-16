#ifndef PV_MOVES_HPP
#define PV_MOVES_HPP

#include "typedefs.hpp"
#include "Move.hpp"

class PvMoves {
    Ply::arrayOf< Ply::arrayOf<Move> > pv;

public:
    PvMoves () { clear(); }

    void clear() {
        FOR_EACH(Ply, ply) {
            pv[ply][0] = Move{};
        }
    }

    void set(Ply ply, Move move) {
        assert (move.isExternal());
        pv[ply][0] = move;
        Move* target = &pv[ply][1];
        Move* source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = Move{};
    }

    operator const Move* () const { return &pv[0][0]; }

    friend out::ostream& operator << (out::ostream& out, const PvMoves& pvMoves) {
        return out << &pvMoves.pv[0][0];
    }

};

#endif
