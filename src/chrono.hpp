#ifndef CHRONO_HPP
#define CHRONO_HPP

#include <chrono>
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

typedef std::chrono::steady_clock clock_type;
typedef clock_type::time_point TimePoint;
typedef clock_type::duration TimeInterval;

inline TimePoint timeNow() {
    return clock_type::now();
}

// ::timeNow() - start
inline TimeInterval elapsedSince(TimePoint start) {
    return ::timeNow() - start;
}

// ::timeNow() + timeInterval
inline TimePoint timeIn(TimeInterval timeInterval) {
    return ::timeNow() + timeInterval;
}

#endif
