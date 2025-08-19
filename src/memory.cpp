#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "memory.hpp"

#ifdef _WIN32
size_t getAvailableMemory() {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullAvailPhys;
}
#else
size_t getAvailableMemory() {
    auto pages = ::sysconf(_SC_AVPHYS_PAGES);
    auto pageSize = ::sysconf(_SC_PAGESIZE);

    if (pages > 0 && pageSize > 0) {
        return static_cast<size_t>(pages) * static_cast<size_t>(pageSize);
    }

    return 0;
}
#endif

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
