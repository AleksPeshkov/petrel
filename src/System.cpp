#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "System.hpp"

namespace System {
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
#ifdef _WIN32
        return _aligned_malloc(size, alignment);  // Use Windows-specific function
#elif defined(_ISOC11_SOURCE)
        return ::aligned_alloc(alignment, size);
#elif _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
        void* result = nullptr;
        if (::posix_memalign(&result, alignment, size) == 0) {
            return result;
        }
        return nullptr;
#else
        // Fallback for systems without aligned allocation
        return std::malloc(size);
#endif
    }

    void freeAligned(void* p) {
#ifdef _WIN32
        _aligned_free(p);  // Use Windows-specific function
#else
        std::free(p);
#endif
    }

    int getPid() {
#ifdef _WIN32
        DWORD pid = GetCurrentProcessId();
#else
        pid_t pid = getpid();
#endif

        return static_cast<int>(pid);
    }

} // end of namespace sys
