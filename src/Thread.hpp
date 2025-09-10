#ifndef THREAD_HPP
#define THREAD_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

using ThreadTask = std::function<void()>;

class Thread {
    enum class Status {
        Ready, // the thread is ready to get a new task
        Start, // the thread has started working on a task
        Stop,  // the thread has received a stop signal
        Abort  // the thread has received an abort signal
    };
    std::atomic<Status> status = Status::Ready;

    std::mutex readyMutex;
    std::condition_variable ready_;
    std::mutex stopMutex;
    std::condition_variable stop_;

    std::thread stdThread;
    ThreadTask threadTask = nullptr;

public:
    Thread();
    ~Thread();

    bool isReady() const;
    void waitReady();

    void start(ThreadTask task);

    void stop();
    bool isStopped() const;
    void waitStop();
};

#endif
