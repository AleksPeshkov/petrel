#ifndef PERFT_RECORD_HPP
#define PERFT_RECORD_HPP

#include "typedefs.hpp"
#include "HashAge.hpp"
#include "Zobrist.hpp"

class PerftRecord {
    typedef unsigned age_t;

    Zobrist::_t key;
    node_count_t nodes;

    enum { DepthBits = 6, DepthShift = 64 - DepthBits, AgeShift = DepthShift - HashAge::AgeBits };

    static const node_count_t DepthMask = static_cast<node_count_t>((1 << DepthBits)-1) << DepthShift;
    static const node_count_t AgeMask = static_cast<node_count_t>((1 << HashAge::AgeBits)-1) << AgeShift;
    static const node_count_t NodesMask = DepthMask | AgeMask;

    static constexpr node_count_t createNodes(node_count_t n, Ply d, HashAge::_t age) {
        //assert (n == (n & ~NodesMask));
        return (n & ~NodesMask) | (static_cast<decltype(nodes)>(age) << AgeShift) | (static_cast<decltype(nodes)>(d) << DepthShift);
    }

public:
    bool isKeyMatch(Zobrist::_t z, Ply d) const {
        return (getKey() == z) && (getDepth() == d);
    }

    bool isAgeMatch(HashAge::_t age) const {
        return ((nodes & AgeMask) >> AgeShift) == age;
    }

    const Zobrist::_t& getKey() const {
        return key;
    }

    Ply getDepth() const {
        return Ply{static_cast<Ply::_t>((nodes & DepthMask) >> DepthShift)};
    }

    node_count_t getNodes() const {
        return nodes & ~NodesMask;
    }

    void set(Zobrist::_t z, Ply d, node_count_t n, HashAge::_t age) {
        key = z;
        nodes = createNodes(n, d, age);
    }

    void setAge(HashAge::_t age) {
        nodes = (nodes & ~AgeMask) | (static_cast<decltype(nodes)>(age) << AgeShift);
    }

};

#endif
