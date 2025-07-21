#ifndef PERFT_TT_HPP
#define PERFT_TT_HPP

#include "Tt.hpp"

class TtPerft : public Tt {
public:
    node_count_t get(const Zobrist&, Ply);
    void set(const Zobrist&, Ply, node_count_t);
};

#endif
