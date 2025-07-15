#ifndef THREAD_CONTROL_HPP
#define THREAD_CONTROL_HPP

#include <condition_variable>
#include "SpinLock.hpp"

class ThreadControl {
public:
    enum class TaskId : unsigned { None };

private:
    /**
     * Idle: the thread is idle
     * Working: the thread is started working on a task with the current TaskId
     * Stopping: the thread is working but going to finish soon and became Idle again
     **/
    enum class Status { Idle, Working, Stopping };

    std::condition_variable_any statusChanged;

    Status status;
    TaskId taskId;

    SpinLock statusLock;
    typedef std::unique_lock<decltype(statusLock)> Guard;

    bool isStatus(Status to) const { return status == to; }

    template <typename Condition> void wait(Condition);
    template <typename Condition> void signal(Status, Condition);
    template <typename Condition> TaskId signalSequence(Status,  Condition);

    void wait(Status to);
    void signal(Status to);
    void signal(Status from, Status to);
    void signal(TaskId id, Status from, Status to);
    TaskId signalSequence(Status from, Status to);

    virtual void run() = 0;

public:
    ThreadControl ();
    virtual ~ThreadControl() { stop(); }

    bool isIdle() const { return isStatus(Status::Idle); }
    bool isRunning() const { return isStatus(Status::Working); }
    bool isStopped() const { return isStatus(Status::Stopping); }
    bool isTask(TaskId id) const { return taskId == id; }

    TaskId start() { return signalSequence(Status::Idle, Status::Working); }
    void stop() { signal(Status::Working, Status::Stopping); wait(Status::Idle); }
    void stop(TaskId id) { signal(id, Status::Working, Status::Stopping); }
};

#endif
