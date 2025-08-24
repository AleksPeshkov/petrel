#include "assert.hpp"
#include "Thread.hpp"

TimerThread::TimerThread() : stdThread{ std::thread([this] {
    while (!isAborting) {
        ScopedLock lock{mutex};

        if (!isActive()) {
            auto condition = [this] { return isAborting || isActive(); };
            timerChanged.wait(lock, condition); // wait for a new schedule
            continue;
        }

        if (::timeNow() < triggerTime) {
            auto condition = [this] { return isAborting || !isActive() || ::timeNow() >= triggerTime; };
            timerChanged.wait_until(lock, triggerTime, condition); // sleep until trigger time
            continue;
        }

        lock.unlock();

        if (task) {
            task();
            task = nullptr;
        }
    }
}) } {}

TimerThread::~TimerThread() {
    {
        ScopedLock lock{mutex};
        isAborting = true;
        triggerTime = TimePoint::max();
        task = nullptr;
    }
    timerChanged.notify_all();

    if (stdThread.joinable()) { stdThread.join(); }
}

void TimerThread::scheduleTask(TimePoint timePoint, ThreadTask timerTask) {
    {
        ScopedLock lock{mutex};
        triggerTime = timePoint;
        task = std::move(timerTask);
    }
    timerChanged.notify_all();
}

template <typename Condition>
void Thread::wait(Condition condition) {
    if (!condition()) {
        ScopedLock lock{mutex};
        statusChanged.wait(lock, condition);
    }
}

template <typename Condition>
void Thread::signal(Condition condition, Status to) {
    {
        ScopedLock lock{mutex};
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
        ScopedLock lock{mutex};
        status = Status::Abort;
    }
    statusChanged.notify_all();
    if (stdThread.joinable()) { stdThread.join(); }
}

void Thread::start(ThreadTask task) {
    assert (isReady());
    {
        ScopedLock g{mutex};
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
