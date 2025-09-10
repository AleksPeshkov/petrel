#ifndef PERFT_TT_HPP
#define PERFT_TT_HPP

#include "Tt.hpp"

class TtPerft : public Tt {
public:
    node_count_t get(ZArg, Ply);
    void set(ZArg, Ply, node_count_t);
};

#endif
