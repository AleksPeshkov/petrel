#ifndef ASSERT_HPP
#define ASSERT_HPP

#ifdef NDEBUG
#include <cassert>
#else

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        } \
    } while (false)

void assert_fail(const char *assertion, const char *file, unsigned int line, const char *function);

#endif // NDEBUG
#endif // ASSERT_HPP
