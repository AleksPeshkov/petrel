#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "typedefs.hpp"

size_t getAvailableMemory();
void* allocateAligned(size_t size, size_t alignment);
void  freeAligned(void*);

#endif
