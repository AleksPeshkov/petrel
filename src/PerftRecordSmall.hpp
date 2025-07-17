#ifndef PERFT_RECORD_SMALL_HPP
#define PERFT_RECORD_SMALL_HPP

#include "typedefs.hpp"
#include "HashAge.hpp"
#include "Zobrist.hpp"

class PerftRecordSmall {
    u32_t key;
    u32_t perft;

public:
    void set(Zobrist::_t z, Ply d, node_count_t n) {
        assert (small_cast<decltype(perft)>(n) == n);
        perft = static_cast<decltype(perft)>(n);

        key = makeKey(z, d);

        assert (getNodes() == n);
        assert (getDepth() == d);
    }

    static u32_t makeKey(Zobrist::_t z, Ply d) {
        assert (d == (d & 0xf));
        return ((static_cast<decltype(key)>(z >> 32) | 0xf) ^ 0xf) | (d & 0xf);
    }

    bool isKeyMatch(Zobrist::_t z, Ply d) const {
        return key == makeKey(z, d);
    }

    node_count_t getNodes() const {
        return perft;
    }

    Ply getDepth() const {
        return key & 0xf;
    }

};

#endif
