#ifndef TIMER_HPP
#define TIMER_HPP

#include "chrono.hpp"
#include "Pool.hpp"
#include "SpinLock.hpp"
#include "ThreadControl.hpp"

class TimerThread;
typedef Pool<TimerThread, SpinLock> TimerPool;

class TimerThread : private ThreadControl {
    friend class Timer;

    TimeInterval timeInterval;
    ThreadControl* thread;
    TimerPool* pool;
    TimerPool::Iterator iterator;
    TaskId taskId;

    void run() override;
};

class Timer : private TimerPool {
public:
    void start(TimeInterval, ThreadControl&, ThreadControl::TaskId);
};

#endif
