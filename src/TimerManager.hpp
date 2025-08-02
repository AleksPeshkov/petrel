#ifndef TIMER_HPP
#define TIMER_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include "chrono.hpp"

class TimerManager {
    struct TimerRecord {
        TimePoint triggerTime;
        std::function<void()> task;

        bool operator < (const TimerRecord& other) const {
            return triggerTime > other.triggerTime; // Min-heap based on due time
        }
    };

    std::priority_queue<TimerRecord> timers;

    std::mutex mutex;
    typedef std::unique_lock<decltype(mutex)> Guard;
    std::condition_variable timersChanged;

    std::thread stdThread;
    bool abort = false;

public:
    TimerManager() {
        stdThread = std::thread([this] {
            while (!abort) {
                Guard lock(mutex);

                if (timers.empty()) {
                    timersChanged.wait(lock); // wait for new timers
                    continue;
                }

                auto triggerTime = timers.top().triggerTime;
                if (triggerTime > ::timeNow()) {
                    timersChanged.wait_until(lock, triggerTime); // sleep until next timer
                    continue;
                }

                auto task = std::move(timers.top().task);
                timers.pop();
                lock.unlock();

                task();
            }
        });
    }

    ~TimerManager() {
        {
            Guard lock(mutex);
            abort = true;
        }
        timersChanged.notify_all();

        if (stdThread.joinable()) stdThread.join();
    }

    void schedule(TimeInterval timeInterval, std::function<void()> task) {
        {
            Guard lock(mutex);
            timers.emplace(::timeIn(timeInterval), std::move(task));
        }
        timersChanged.notify_all();
    }
};

extern TimerManager The_timerManager;

#endif
