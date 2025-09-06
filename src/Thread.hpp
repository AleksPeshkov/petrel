#ifndef THREAD_HPP
#define THREAD_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

typedef std::function<void()> ThreadTask;

class Thread {
    enum class Status {
        Ready, // ready to accept a new task
        Start, // started working on a task
        Finish,// finished working on a task
        Stop,  // signal to stop a task as fast as possible
        Abort  // abort signal on program exit
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
    void stopIfFinished();
    bool isStopped() const;

    void finishedWaitStop();
};

#endif
