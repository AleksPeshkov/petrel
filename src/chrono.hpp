#ifndef CHRONO_HPP
#define CHRONO_HPP

#include <chrono>
#include "io.hpp"

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

inline istream& operator >> (istream& in, TimeInterval& timeInterval) {
    unsigned long msecs;
    if (in >> msecs) {
        timeInterval = duration_cast<TimeInterval>(milliseconds{msecs} );
    }
    return in;
}

inline ostream& operator << (ostream& out, const TimeInterval& timeInterval) {
    return out << duration_cast<milliseconds>(timeInterval).count();
}

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
}

#endif
