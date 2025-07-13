#ifndef PV_MOVES_HPP
#define PV_MOVES_HPP

#include "typedefs.hpp"
#include "Move.hpp"

class PvMoves {
    enum { N = ::MaxDepth };
    std::array< std::array<Move, N>, N> pv;

public:
    PvMoves () { clear(); }

    void clear() {
        for (index_t ply = 0; ply < N; ++ply) {
            pv[ply][0] = Move{};
        }
    }

    void operator() (ply_t ply, Move move) {
        assert (ply < N-1);
        pv[ply][0] = move;
        Move* target = &pv[ply][1];
        Move* source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = Move{};
    }

    operator const Move* () const { return &pv[0][0]; }

};

#endif
