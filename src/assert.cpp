#include <errno.h>
#include "assert.hpp"
#include "io.hpp"

#ifndef NDEBUG

void assert_fail(const char *assertion, const char *file, unsigned int line, const char* func) {
    std::ostringstream oss;
    oss << "Assertion failed: " << func << ": " << assertion << " (" << file << ":" << line << ")";
    std::string message = oss.str();
    io::log("#" + message);
    std::cerr << message << std::endl;

#if defined(__GNUC__)
    __builtin_trap();
#endif

    std::exit(EXIT_FAILURE); // graceful exit without core dump

#if defined(__GNUC__)
    __builtin_unreachable();
#endif
}

#endif
