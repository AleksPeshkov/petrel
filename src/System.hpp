#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "bitops.hpp"

namespace System {
    size_t getAvailableMemory();
    void* allocateAligned(size_t size, size_t alignment);
    void  freeAligned(void*);
    int getPid();
}

#endif
