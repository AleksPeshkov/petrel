#ifndef CHRONO_HPP
#define CHRONO_HPP

#include <chrono>
using std::chrono::duration_cast;

typedef std::chrono::microseconds TimeInterval; // internal engine time duration unit
typedef std::chrono::milliseconds Msecs; // UCI time duration unit for input and output

template <typename nodes_type, typename duration_type>
constexpr nodes_type nps(nodes_type nodes, duration_type duration) {
    return (nodes * duration_type::period::den) / (static_cast<nodes_type>(duration.count()) * duration_type::period::num);
}

class TimePoint {
    typedef std::chrono::steady_clock clock_type;
    std::chrono::time_point<clock_type> start;

public:
    TimePoint () : start( clock_type::now() ) {}

    TimeInterval getDuration() const {
        return duration_cast<TimeInterval>(clock_type::now() - start);
    }
};

#endif
