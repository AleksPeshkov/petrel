#ifndef PV_MOVES_HPP
#define PV_MOVES_HPP

#include "typedefs.hpp"
#include "UciMove.hpp"

class CACHE_ALIGN PvMoves {
    Ply::arrayOf< Ply::arrayOf<UciMove> > pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void set(Ply ply, UciMove move) {
        pv[ply][0] = move;
        auto target = &pv[ply][1];
        auto source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = {};
    }

    operator const UciMove* () const { return &pv[0][0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << &pvMoves.pv[0][0];
    }

};

#endif
