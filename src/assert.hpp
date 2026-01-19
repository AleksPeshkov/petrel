#ifndef ASSERT_HPP
#define ASSERT_HPP

#ifdef NDEBUG
#define assert(expr) (static_cast<void>(0))
#else

#ifdef ENABLE_ASSERT_LOGGING
// custom assert fail with logging
void assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) __attribute__ ((__noreturn__));

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        } \
    } while (false)

#else

#include <cassert>

#endif // ENABLE_ASSERT_LOGGING
#endif // NDEBUG
#endif // ASSERT_HPP
