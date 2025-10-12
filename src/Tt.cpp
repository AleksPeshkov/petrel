#include "Tt.hpp"
#include "bitops.hpp"
#include "memory.hpp"
#include "Zobrist.hpp"

size_t Tt::maxSize() {
    return ::bit_floor(::getAvailableMemory());
}

void Tt::free() {
    if (size_) {
        ::freeAligned(memory);
        memory = nullptr;
        size_ = 0;
    }
}

void Tt::zeroFill() {
    std::memset(memory, 0, size_);
}

void Tt::allocate(size_t _bytes) {
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
