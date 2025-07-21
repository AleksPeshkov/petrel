#include "Tt.hpp"
#include "bitops.hpp"
#include "memory.hpp"

size_t Tt::getMaxSize() const {
    return ::round(::getAvailableMemory());
}
