#ifndef ASSERT_HPP
#define ASSERT_HPP

#ifdef NDEBUG
#include <cassert>
#else

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__); \
        } \
    } while (false)

void assert_fail(const char *assertion, const char *file, unsigned int line);
void setup_signal_handler();

#endif

#endif
