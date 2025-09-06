#ifndef THREAD_HPP
#define THREAD_HPP

#include <atomic>
#include <functional>
#include <thread>

using ThreadTask = std::function<void()>;

class Thread {
    enum class Status {
        Ready, // ready to accept a new task
        Busy,  // busy working on a task
        Abort  // abort signal on program exit
    };
    std::atomic<Status> status{Status::Ready};

    ThreadTask threadTask{nullptr};
    std::thread stdThread;

    Status getStatus() const { return status.load(std::memory_order_acquire); }
    void waitStatus(Status old) { status.wait(old, std::memory_order_acquire); }
    void setStatus(Status desired) { status.store(desired, std::memory_order_release); status.notify_all(); }

public:
    Thread() : stdThread([this] {
        while (getStatus() != Status::Abort) {
            threadTask = nullptr;
            setStatus(Status::Ready);
            waitStatus(Status::Ready);

            if (getStatus() == Status::Busy && threadTask) {
                threadTask();
            }
        }
    }) {}

    ~Thread() {
        setStatus(Status::Abort);
        if (stdThread.joinable()) { stdThread.join(); }
    }

    void waitReady() {
        while (true) {
            auto current = getStatus();
            if (current != Status::Busy) { break; }
            waitStatus(current);
        }
    }

    bool start(ThreadTask&& task) {
        if (getStatus() != Status::Ready || threadTask != nullptr) { return false; }

        threadTask = std::move(task);
        setStatus(Status::Busy);
        return true;
    }
};

#endif
