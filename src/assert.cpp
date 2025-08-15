#include "assert.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include "io.hpp"

#ifndef NDEBUG

namespace {
    void handle_sigtrap(int) {
        std::exit(EXIT_FAILURE); // graceful exit without core dump
    }
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_sigtrap;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTRAP, &sa, nullptr);
}

void assert_fail(const char *assertion, const char *file, unsigned int line) {
    std::ostringstream oss;
    oss << "Assertion failed: " << assertion << " (" << file << ":" << line << ")";
    std::string message = oss.str();
    io::log("#" + message);
    std::cerr << message << std::endl;

    raise(SIGTRAP); // trigger debugger or invoke signal handler
    __builtin_unreachable();
}

#endif
