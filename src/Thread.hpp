#ifndef THREAD_HPP
#define THREAD_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "chrono.hpp"

typedef std::function<void()> ThreadTask;

class TimerThread {
    TimePoint  triggerTime = TimePoint::max();
    ThreadTask task;

    std::mutex mutex;
    typedef std::unique_lock<decltype(mutex)> ScopedLock;
    std::condition_variable timerChanged;

    std::thread stdThread;
    bool isAborting = false; // the thread is being destructed on program termination

    constexpr bool isActive() const { return triggerTime != TimePoint::max(); }

public:
    TimerThread();
    ~TimerThread();

    void scheduleTask(TimePoint, ThreadTask);
    void cancelTask() { scheduleTask(TimePoint::max(), nullptr); }
};

class Thread {
    enum class Status {
        Ready, // the thread is ready to get a new task
        Run,   // the thread is busy working on a task
        Stop,  // the thread has received a stop signal
        Abort  // the thread has received an abort signal
    };

    std::thread stdThread;
    ThreadTask threadTask = nullptr;

    Status status = Status::Ready;
    std::mutex mutex;
    typedef std::unique_lock<decltype(mutex)> ScopedLock;
    std::condition_variable statusChanged;

    bool is(Status to) const { return status == to; }
    template <typename Condition> void wait(Condition);
    template <typename Condition> void signal(Condition, Status);

public:
    Thread();
    ~Thread();

    bool isReady()   const { return is(Status::Ready); }
    bool isStopped() const { return is(Status::Stop) || is(Status::Abort); }

    void start(ThreadTask task);
    void stop();
    void waitStop();
};

class ThreadWithDeadline : public Thread {
    TimerThread timer;

public:
    void start(ThreadTask task) {
        Thread::start([this, task]() {
            task();
            timer.cancelTask();
        });
    }
    void setTaskDeadline(TimePoint deadline) { timer.scheduleTask(deadline, [this]() { stop(); }); }
};

#endif
