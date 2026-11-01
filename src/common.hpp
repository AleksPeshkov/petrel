#ifndef COMMON_HPP
#define COMMON_HPP

#define COLD __attribute__((cold))

#ifdef assert
#undef assert
#endif

#ifndef NDEBUG

// custom assert() with logging, defined in main.cpp
COLD void assert_fail(const char* assertion, const char* file, unsigned int line, const char* function) __attribute__ ((__noreturn__));

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        } \
    } while (false)

#else

#define assert(expr) (static_cast<void>(0))

#endif // NDEBUG

#if defined(__clang__)
    #define ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
    #if __GNUC__ >= 13
        #define ASSUME(cond) [[gnu::assume(cond)]]
    #else
        #define ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
    #endif
#else
    #define ASSUME(cond) do { (void)sizeof(cond); } while (0)
#endif

#endif
