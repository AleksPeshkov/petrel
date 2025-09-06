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

public:
    Thread() : stdThread([this] {
        for (;;) {
            // wait for Status::Busy
            while (true) {
                auto current = status.load(std::memory_order_acquire);
                if (current == Status::Abort) { return; }
                if (current == Status::Busy) { break; }
                status.wait(current, std::memory_order_acquire);
            }

            if (threadTask) {
                threadTask();
                threadTask = nullptr;
            }

            auto current = status.load(std::memory_order_acquire);
            if (current == Status::Abort) { return; }

            status.store(Status::Ready, std::memory_order_release);
        }
    }) {}

    ~Thread() {
        status.store(Status::Abort, std::memory_order_release);
        status.notify_all();
        if (stdThread.joinable()) { stdThread.join(); }
    }

    void start(ThreadTask&& task) {
        threadTask = std::move(task);
        status.store(Status::Busy, std::memory_order_release);
        status.notify_one(); // wake up the worker thread
    }
};

#endif
