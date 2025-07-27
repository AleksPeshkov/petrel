#ifndef TT_HPP
#define TT_HPP

#include "TtMemory.hpp"

class HashAge {
public:
    typedef unsigned _t;
    enum {AgeBits = 3, AgeMask = (1u << AgeBits)-1};

private:
    _t v;

public:
    constexpr HashAge () : v(1) {}
    constexpr operator const _t& () { return v; }

    void nextAge() {
        //there are "AgeMask" ages, not "1 << AgeBits", because of:
        //1) we want to break 4*n ply transposition pattern
        //2) make sure that initally clear entry is never hidden
        auto a = (v + 1) & AgeMask;
        v = a ? a : 1;
    }

};

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
