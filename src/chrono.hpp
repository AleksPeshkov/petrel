#ifndef CHRONO_HPP
#define CHRONO_HPP

#include <chrono>
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

typedef std::chrono::steady_clock Clock;
typedef Clock::time_point TimePoint;
typedef Clock::duration TimeInterval;

inline TimeInterval elapsedSince(TimePoint start) {
    return Clock::now() - start;
}

// time point since now
inline TimePoint timeIn(TimeInterval timeInterval) {
    return Clock::now() + timeInterval;
}

#endif
