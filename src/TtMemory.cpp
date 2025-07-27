#include <algorithm>
#include <cstring>
#include "memory.hpp"
#include "TtMemory.hpp"
#include "Zobrist.hpp"

void TtMemory::free() {
    if (mask) {
        ::free(memory);
        memory = &bucket;
        size = BucketSize;
        mask = 0;
    }
}

size_t TtMemory::allocate(size_t _bytes) {
    auto bytes = ::round(_bytes);

    if (bytes != size) {
        free();

        for (; bytes > BucketSize; bytes >>= 1) {
            auto ptr = ::allocateAligned(bytes, MinHashSize);

            if (ptr) {
                memory = static_cast<TtBucket*>(ptr);
                size = bytes;
                mask = (size-1) ^ (BucketSize-1);
                break;
            }
        }
    }

    assert (bytes == size);
    zeroFill();
    return size;
}

void TtMemory::zeroFill() {
    std::memset(memory, 0, size);
}


constexpr TtBucket* TtMemory::probe(const Z& z) const {
    auto ptr = static_cast<char*>(static_cast<void*>(memory));
    return static_cast<TtBucket*>(static_cast<void*>(ptr + (z & mask)));
}

TtBucket* TtMemory::prefetch(const Z& z) const {
    auto ptr = probe(z);
    _mm_prefetch(ptr, _MM_HINT_NTA);
    return ptr;
}
