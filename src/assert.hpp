#ifndef ASSERT_HPP
#define ASSERT_HPP

#ifndef NDEBUG

// custom assert() with logging, defined in main.cpp
void assert_fail(const char* assertion, const char* file, unsigned int line, const char* function) __attribute__ ((__noreturn__));

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        } \
    } while (false)

#else
#define assert(expr) (static_cast<void>(0))
#endif // NDEBUG

#endif // ASSERT_HPP
