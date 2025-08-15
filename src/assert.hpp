#ifndef ASSERT_HPP
#define ASSERT_HPP

#ifdef NDEBUG

#include <cassert>

#else

#include <iostream>
#include <stdlib.h>
#include <string>

extern void log(const std::string&);

#define assert(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #expr << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::string message = oss.str(); \
            log(message); \
            std::cerr << message << std::endl; \
            std::abort(); \
        } \
    } while (0)

#endif

#endif
