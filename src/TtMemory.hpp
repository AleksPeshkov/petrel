#ifndef TT_MEMORY_HPP
#define TT_MEMORY_HPP

#include <cstdint>
#include "TtBucket.hpp"

class Z;

class TtMemory {
    // default bucket for testing without transposition table
    TtBucket bucket;

public:
    static constexpr size_t MinHashSize = 1024 * 1024;
    static constexpr size_t BucketSize = sizeof(TtBucket);

protected:
    TtBucket* memory = &bucket;
    std::uintptr_t mask = 0;
    size_t size = BucketSize;

    void free();

public:
    TtMemory() {}
    ~TtMemory() { free(); }

    constexpr const size_t& getSize() const { return size; }

    size_t allocate(size_t);
    void zeroFill();

    constexpr TtBucket* probe(const Z&) const;
    TtBucket* prefetch(const Z&) const;
};

#endif
