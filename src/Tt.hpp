#ifndef TT_HPP
#define TT_HPP

#include "HashAge.hpp"
#include "typedefs.hpp"
#include "Zobrist.hpp"

class TtBucket;

class Tt {
    void* memory = nullptr;
    size_t size_ = 0;

    void allocate(size_t);
    void free();
    void zeroFill();
    constexpr uintptr_t mask(size_t align) const { return (size_-1) ^ (align-1); }

public:
    node_count_t reads = 0;
    node_count_t writes = 0;
    node_count_t hits = 0;

    HashAge hashAge;

    Tt() { setSize(minSize()); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }

    // 1MB
    constexpr size_t minSize() const { return 1024 * 1024; }
    static size_t maxSize();

    void setSize(size_t bytes) { allocate(bytes); newGame(); }
    void newGame() { zeroFill(); hashAge = {}; }
    void newSearch() { reads = 0; writes = 0; hits = 0; newIteration(); }
    void newIteration() { hashAge.nextAge(); }

    const HashAge& getAge() const { return hashAge; }
    void nextAge() { hashAge.nextAge(); }

    constexpr void* addr(const Z& z, size_t align) const {
        return static_cast<void*>(static_cast<char*>(memory) + (z & mask(align)));
    }

    template <typename T> constexpr T* addr(const Z& z) const {
        return static_cast<T*>( addr(z, sizeof(T)) );
    }

    void* prefetch(const Z& z, size_t align) const {
        auto ptr = addr(z, align);
        __builtin_prefetch(ptr);
        return ptr;
    }

    template <typename T> T* prefetch(const Z& z) const {
        return static_cast<T*>( prefetch(z, sizeof(T)) );
    }

};

#endif
