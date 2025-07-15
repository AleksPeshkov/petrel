#ifndef TIMER_HPP
#define TIMER_HPP

#include "Duration.hpp"
#include "Pool.hpp"
#include "SpinLock.hpp"
#include "ThreadControl.hpp"

class TimerThread;
typedef Pool<TimerThread, SpinLock> TimerPool;

class TimerThread : private ThreadControl {
    friend class Timer;

    Duration duration;
    ThreadControl* thread;
    TimerPool* pool;
    TimerPool::Iterator iterator;
    TaskId taskId;

    void run() override;
};

class Timer : private TimerPool {
public:
    void start(Duration, ThreadControl&, ThreadControl::TaskId);
};

#endif
