#ifndef CHRONO_HPP
#define CHRONO_HPP

#include <chrono>
#include "io.hpp"

using namespace std::chrono_literals;

using clock_type = std::chrono::steady_clock;
using TimePoint = clock_type::time_point;
using TimeInterval = clock_type::duration;

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
    int msecs;
    if (in >> msecs) {
        timeInterval = std::chrono::duration_cast<TimeInterval>(std::chrono::milliseconds{msecs} );
    }
    return in;
}

inline ostream& operator << (ostream& out, const TimeInterval& timeInterval) {
    return out << std::chrono::duration_cast<std::chrono::milliseconds>(timeInterval).count();
}

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
}

#endif
