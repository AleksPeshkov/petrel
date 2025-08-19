#include <cstdlib>
#include <unistd.h>
#include "memory.hpp"

size_t getPageSize() {
    return static_cast<size_t>(::sysconf(_SC_PAGESIZE));
}

size_t getAvailableMemory() {
    auto pages = ::sysconf(_SC_AVPHYS_PAGES);
    auto pageSize = getPageSize();

    if (pages > 0 && pageSize > 0) {
        return static_cast<size_t>(pages) * pageSize;
    }

    return 0;
}

void* allocateAligned(size_t size, size_t alignment) {
#ifdef _ISOC11_SOURCE
    return ::aligned_alloc(alignment, size);
#else
#   if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
    void* result = nullptr;
    ::posix_memalign(&result, alignment, size);
    return result;
#   endif
#endif
}

void freeAligned(void* p) {
    std::free(p);
}
