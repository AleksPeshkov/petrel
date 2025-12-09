#ifndef TT_HPP
#define TT_HPP

#include "memory.hpp"
#include "Zobrist.hpp"

class Tt {
    void* memory = nullptr;
    size_t size_ = 0;

    void free() {
        if (size_) {
            ::freeAligned(memory);
            memory = nullptr;
            size_ = 0;
        }
    }

    void zeroFill() {
        std::memset(memory, 0, size_);
    }

    void allocate(size_t _bytes) {
        const auto minBytes = minSize();
        auto bytes = ::bit_floor(std::max(_bytes, minBytes));

        if (bytes != size_) {
            free();

            for (; bytes >= minBytes; bytes >>= 1) {
                auto ptr = ::allocateAligned(bytes, minBytes);

                if (ptr) {
                    memory = ptr;
                    size_ = bytes;
                    break;
                }
            }
        }

        assert (bytes == size_);
        zeroFill();
    }

    constexpr uintptr_t mask(size_t align) const { return (size_-1) ^ (align-1); }

public:
    node_count_t reads = 0;
    node_count_t writes = 0;
    node_count_t hits = 0;

    Tt(size_t n = minSize()) { setSize(n); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }

    // 2MB to trigger linux huge page support if possible
    static constexpr size_t minSize() { return 2 * 1024 * 1024; }

    // all currently available memory
    static size_t maxSize() { return ::bit_floor(::getAvailableMemory()); }

    void setSize(size_t bytes) { allocate(bytes); newGame(); }
    void newGame() { zeroFill(); newSearch(); }
    void newSearch() { reads = 0; writes = 0; hits = 0; newIteration(); }
    void newIteration() {}

    constexpr void* addr(Z z, size_t align) const {
        return static_cast<void*>(static_cast<char*>(memory) + (z & mask(align)));
    }

    template <typename T>
    constexpr T* addr(Z z) const {
        return static_cast<T*>( addr(z, sizeof(T)) );
    }

    void* prefetch(Z z, size_t align) const {
        auto ptr = addr(z, align);
        __builtin_prefetch(ptr);
        return ptr;
    }

    template <typename T>
    T* prefetch(Z z) const {
        return static_cast<T*>( prefetch(z, sizeof(T)) );
    }

};

#endif
