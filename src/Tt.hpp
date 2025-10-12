#ifndef TT_HPP
#define TT_HPP

#include "typedefs.hpp"
#include "Zobrist.hpp"

class HashAge {
public:
    using _t = int;
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

    Tt(size_t n = minSize()) { setSize(n); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }

    // 2MB to trigger linux huge page support if possible
    static constexpr size_t minSize() { return 2 * 1024 * 1024; }
    static size_t maxSize();

    void setSize(size_t bytes) { allocate(bytes); newGame(); }
    void newGame() { zeroFill(); hashAge = {}; newSearch(); }
    void newSearch() { reads = 0; writes = 0; hits = 0; newIteration(); }
    void newIteration() { hashAge.nextAge(); }

    const HashAge& getAge() const { return hashAge; }
    void nextAge() { hashAge.nextAge(); }

    constexpr void* addr(Z z, size_t align) const {
        return static_cast<void*>(static_cast<char*>(memory) + (z & mask(align)));
    }

    template <typename T> constexpr T* addr(Z z) const {
        return static_cast<T*>( addr(z, sizeof(T)) );
    }

    void* prefetch(Z z, size_t align) const {
        auto ptr = addr(z, align);
        __builtin_prefetch(ptr);
        return ptr;
    }

    template <typename T> T* prefetch(Z z) const {
        return static_cast<T*>( prefetch(z, sizeof(T)) );
    }

};

#endif
