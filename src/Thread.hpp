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
    ThreadTask timerTask;

    std::mutex timerLock;
    typedef std::unique_lock<decltype(timerLock)> Guard;
    std::condition_variable timerChanged;

    std::thread stdThread;
    bool abort = false;

    constexpr bool isActive() const { return triggerTime != TimePoint::max(); }

public:
    TimerThread();
    ~TimerThread();

    void schedule(TimePoint, ThreadTask);
    void cancel() { schedule(TimePoint::max(), nullptr); }
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
    std::mutex statusLock;
    typedef std::unique_lock<decltype(statusLock)> Guard;
    std::condition_variable_any statusChanged;

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
            timer.cancel();
        });
    }
    void setDeadline(TimePoint deadline) { timer.schedule(deadline, [this]() { stop(); }); }
};

#endif
