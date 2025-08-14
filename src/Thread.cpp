#include "assert.hpp"
#include "Thread.hpp"

TimerThread::TimerThread() : stdThread{ std::thread([this] {
    while (!abort) {
        Guard lock{timerLock};

        if (!isActive()) {
            timerChanged.wait(lock); // wait for a new schedule
            continue;
        }

        if (::timeNow() < triggerTime) {
            timerChanged.wait_until(lock, triggerTime); // sleep until trigger time
            continue;
        }

        lock.unlock();

        if (timerTask) {
            timerTask();
            timerTask = nullptr;
        }
    }
}) } {}

TimerThread::~TimerThread() {
    {
        Guard lock{timerLock};
        abort = true;
        triggerTime = TimePoint::max();
        timerTask = nullptr;
    }
    timerChanged.notify_all();

    if (stdThread.joinable()) { stdThread.join(); }
}

void TimerThread::schedule(TimePoint timePoint, ThreadTask task) {
    {
        Guard lock{timerLock};
        triggerTime = timePoint;
        timerTask = std::move(task);
    }
    timerChanged.notify_all();
}

template <typename Condition>
void Thread::wait(Condition condition) {
    if (!condition()) {
        Guard lock{statusLock};
        statusChanged.wait(lock, condition);
    }
}

template <typename Condition>
void Thread::signal(Condition condition, Status to) {
    {
        Guard lock{statusLock};
        if (!condition()) { return; }
        status = to;
    }
    statusChanged.notify_all();
}

Thread::Thread() : stdThread{ std::thread([this] {
    for(;;) {
        signal([this]() { return !is(Status::Ready); }, Status::Ready);
        wait([this] { return !is(Status::Ready); });
        if (is(Status::Abort)) { return; }

        if (threadTask) {
            threadTask();
            threadTask = nullptr;
        }

        if (is(Status::Abort)) { return; }
    }
})}
{}

Thread::~Thread() {
    {
        Guard lock{statusLock};
        status = Status::Abort;
    }
    statusChanged.notify_all();
    if (stdThread.joinable()) { stdThread.join(); }
}

void Thread::start(ThreadTask task) {
    assert (isReady());
    {
        Guard g{statusLock};
        threadTask = std::move(task);
        status = Status::Run;
    }
    statusChanged.notify_all();
}

void Thread::stop() {
    signal([this]() { return !is(Status::Abort); }, Status::Stop);
    wait([this] { return is(Status::Ready); });
}

void Thread::waitStop() {
    wait([this] { return is(Status::Stop) || is(Status::Abort); });
}
