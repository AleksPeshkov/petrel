#include <cstring>
#include <xmmintrin.h>
#include "HashMemory.hpp"
#include "bitops.hpp"
#include "memory.hpp"

HashMemory::HashMemory (size_t bucketSize) :
    BucketSize{ bucketSize },
    PageSize{ std::max(::getPageSize(), static_cast<size_t>(BucketSize)) }
{
    setDefault();
    info.maxSize = round(::getAvailableMemory());
}

void HashMemory::clear() {
    std::memset(hash, 0, info.currentSize);
}

void HashMemory::set(void* _hash, size_t _size) {
    assert (_size == round(_size));

    hash = _hash;
    info.currentSize = _size;
    mask = (_size-1) ^ (BucketSize-1);

    clear();
}

void HashMemory::setDefault() {
    resize(PageSize);
}

void HashMemory::free() {
    ::freeAligned(hash);
    hash = nullptr;
    info.currentSize = 0;
}

void HashMemory::resize(size_t bytes) {
    bytes = round(std::max(PageSize, bytes));

    if (bytes == info.currentSize) {
        clear();
        return;
    }

    free();
    info.maxSize = round(::getAvailableMemory());
    bytes = std::min(bytes, info.maxSize);

    for (; bytes >= PageSize; bytes >>= 1) {
        auto p = ::allocateAligned(bytes, PageSize);

        if (p) {
            set(p, bytes);
            return;
        }
    }

}

void* HashMemory::seek(Zobrist z) const {
    return reinterpret_cast<char*>(hash) + (static_cast<Zobrist::_t>(z) & mask);
}

void* HashMemory::prefetch(Zobrist z) const {
    auto o = seek(z);
    _mm_prefetch(o, _MM_HINT_NTA);
    return o;
}
