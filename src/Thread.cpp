#include "Thread.hpp"

Thread::Thread() : stdThread([this] {
    for (;;) {
        auto currenStatus = status.load(std::memory_order_acquire);
        if (currenStatus == Status::Abort) { return; }

        if (currenStatus != Status::Ready) {
            {
                std::lock_guard<std::mutex> lock(readyMutex);
                currenStatus = status.load(std::memory_order_relaxed);
                if (currenStatus == Status::Abort) { return; }
                if (currenStatus == Status::Ready) { continue; }

                status.store(Status::Ready, std::memory_order_release);
            }
            // signal Ready
            ready_.notify_all();
        }

        // wait for start()
        {
            std::unique_lock<std::mutex> lock(readyMutex);
            ready_.wait(lock, [this] { return !isReady(); });
        }

        if (threadTask && status.load(std::memory_order_acquire) == Status::Start ) {
            threadTask();
            threadTask = nullptr;
        }
    }
}) {}

Thread::~Thread() {
    status.store(Status::Abort, std::memory_order_release);
    ready_.notify_all();
    stop_.notify_all();

    if (stdThread.joinable()) {
        stdThread.join();
    }
}

bool Thread::isReady() const {
    return status.load(std::memory_order_acquire) == Status::Ready;
}

void Thread::waitReady() {
    if (!isReady()) {
        std::unique_lock<std::mutex> lock(readyMutex);
        ready_.wait(lock, [this] { return isReady(); });
    }
}

void Thread::start(ThreadTask task) {
    waitReady();
    {
        std::lock_guard<std::mutex> lock(readyMutex);
        threadTask = std::move(task);
        status.store(Status::Start, std::memory_order_release);
    }
    ready_.notify_all();
}

void Thread::stop() {
    auto currentStatus = status.load(std::memory_order_acquire);
    if (currentStatus != Status::Start && currentStatus != Status::Finish) { return; }

    {
        std::lock_guard<std::mutex> lock(stopMutex);
        currentStatus = status.load(std::memory_order_relaxed);
        if (currentStatus != Status::Start && currentStatus != Status::Finish) { return; }
        status.store(Status::Stop, std::memory_order_release);
    }
    stop_.notify_all();
}

void Thread::stopIfFinished() {
    auto currentStatus = status.load(std::memory_order_acquire);
    if (currentStatus == Status::Finish) { stop(); }
}

bool Thread::isStopped() const {
    auto currentStatus = status.load(std::memory_order_acquire);
    return currentStatus == Status::Stop || currentStatus == Status::Abort;
}

void Thread::finishedWaitStop() {
    auto currentStatus = status.load(std::memory_order_relaxed);
    if (currentStatus == Status::Start) {
        status.store(Status::Finish, std::memory_order_relaxed);
    }

    if (!isStopped()) {
        std::unique_lock<std::mutex> lock(stopMutex);
        stop_.wait(lock, [this] { return isStopped(); });
    }
}
