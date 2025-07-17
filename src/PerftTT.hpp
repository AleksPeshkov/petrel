#ifndef PERFT_TT_HPP
#define PERFT_TT_HPP

#include "HashAge.hpp"
#include "HashMemory.hpp"
#include "Zobrist.hpp"

class HashBucket;

class PerftTT {
    struct Counter {
        node_count_t reads = 0;
        node_count_t writes = 0;
        node_count_t hits = 0;
    } counter;

    HashMemory hashMemory;
    HashAge hashAge;

public:
    PerftTT ();

    node_count_t get(Zobrist, Ply);
    void set(Zobrist, Ply, node_count_t);

    const Counter& getCounter() const { return counter; }

    const HashAge& getAge() const { return hashAge; }
    void nextAge() { hashAge.nextAge(); }

    const HashInfo& getInfo() const { return hashMemory.getInfo(); }
    void resize(size_t bytes) { hashMemory.resize(bytes); }
    void clear() { hashMemory.clear(); clearCounter(); hashAge = {}; }
    void clearCounter() { counter = {0, 0, 0}; }

};

#endif
