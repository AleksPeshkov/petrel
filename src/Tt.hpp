#ifndef TT_HPP
#define TT_HPP

#include "HashAge.hpp"
#include "TtMemory.hpp"
#include "Zobrist.hpp"

class TtBucket;

class Tt {
public:
    struct Counter {
        node_count_t reads = 0;
        node_count_t writes = 0;
        node_count_t hits = 0;
    } counter;

    TtMemory memory;
    HashAge hashAge;

    Tt () = default;

    void newGame() { memory.zeroFill(); hashAge = {}; newSearch(); }
    void newSearch() { counter = {0, 0, 0}; newIteration(); }
    void newIteration() { hashAge.nextAge(); }

    void allocate(size_t bytes) { memory.allocate(bytes); newGame(); }
    const size_t& getSize() const { return memory.getSize(); }
    size_t getMaxSize() const;

    const HashAge& getAge() const { return hashAge; }
    void nextAge() { hashAge.nextAge(); }
};

#endif
