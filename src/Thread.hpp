#ifndef THREAD_HPP
#define THREAD_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "chrono.hpp"

class Thread {
public:
    enum class TaskId : unsigned { None };
    typedef std::function<void()> Task;

    enum class Status {
        Ready, // the thread is ready to get a new task
        Run,  // the thread is busy working on a task with the current taskId
        Stop, // the thread have got the stop signal and is going to be idle soon again
        Abort // the thread have got abort signal
    };

private:
    Status status = Status::Ready;
    std::mutex statusLock;
    std::condition_variable_any statusChanged;
    std::thread stdThread;

    typedef std::unique_lock<decltype(statusLock)> Guard;

    bool is(Status to) const { return status == to; }

    template <typename Condition> void wait(Condition);
    template <typename Condition> void signal(Condition, Status);

    Task threadTask = nullptr;
    TaskId taskId = TaskId::None;

public:
    Thread();
    ~Thread();

    bool isReady()    const { return is(Status::Ready); }
    bool isStopped() const { return is(Status::Stop) || is(Status::Abort); }

    void stop();
    void stop(TaskId id);

    constexpr TaskId getTaskId() const { return taskId; }

    TaskId start(Task task);

    void waitStop();
};

#endif
