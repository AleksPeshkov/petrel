#ifndef SPIN_LOCK_HPP
#define SPIN_LOCK_HPP

#include <atomic>
#include <immintrin.h>

class SpinLock {
    std::atomic_flag atomic = ATOMIC_FLAG_INIT;

public:

    bool try_lock() {
        return !atomic.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        atomic.clear(std::memory_order_release);
    }

    void lock() {
        while (!try_lock()) {
            _mm_pause();
        }
    }

};

#endif
