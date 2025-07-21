#include <algorithm>
#include <cstring>
#include "Tt.hpp"
#include "bitops.hpp"
#include "memory.hpp"
#include "Zobrist.hpp"

size_t Tt::maxSize() {
    return ::round(::getAvailableMemory());
}

void Tt::free() {
    if (size_) {
        ::free(memory);
        memory = nullptr;
        size_ = 0;
    }
}

void Tt::zeroFill() {
    std::memset(memory, 0, size_);
}

void Tt::allocate(size_t _bytes) {
    const auto min = minSize();
    auto bytes = ::round(std::max(_bytes, min));

    if (bytes != size_) {
        free();

        for (; bytes >= min; bytes >>= 1) {
            auto ptr = ::allocateAligned(bytes, min);

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
