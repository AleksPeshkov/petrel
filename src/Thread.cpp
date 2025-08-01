#include <thread>
#include "Thread.hpp"

namespace {
    using Id = Thread::TaskId;
    Id& operator++ (Id& id) {
        id = static_cast<Id>( static_cast< std::underlying_type_t<Id> >(id)+1 );
        if (id == Id::None) {
            //wrap around Id::None
            id = static_cast<Id>( static_cast< std::underlying_type_t<Id> >(Id::None)+1 );
        }
        return id;
    }
}

template <typename Condition>
void Thread::wait(Condition condition) {
    if (!condition()) {
        Guard g{statusLock};
        statusChanged.wait(g, condition);
    }
}

template <typename Condition>
void Thread::signal(Condition condition, Status to) {
    {
        Guard g{statusLock};
        if (!condition()) { return; }
        status = to;
    }
    statusChanged.notify_all();
}

Thread::Thread() {
    auto infiniteLoop = [this] {
        for (;;) {
            wait([this] { return isStatus(Status::Working); });
            if (threadTask) {
                threadTask();
            }
            threadTask = nullptr;
            signal([this]() { return !isStatus(Status::Idle); }, Status::Idle);
        }
    };
    std::thread(infiniteLoop).detach();
}

Thread::TaskId Thread::start(Task task) {
    TaskId result;
    {
        Guard g{statusLock};
        if (!isStatus(Status::Idle)) { return TaskId::None; }

        threadTask = std::move(task);
        result = ++taskId;
        status = Status::Working;
    }
    statusChanged.notify_all();
    return result;
}

void Thread::startTimer(TaskId id, TimeInterval timeInterval) {
    if (id != taskId || timeInterval == TimeInterval::zero()) { return; }

    auto iterator = timerPool.acquire();
    auto& timerThread = timerPool.fetch(iterator);

    auto task = [this, timeInterval, id, iterator]() mutable {
        std::this_thread::sleep_for(timeInterval);
        stop(id);
        timerPool.release(std::move(iterator));
    };

    timerThread.start(std::move(task));
}

void Thread::stop() {
    signal([this]() { return isStatus(Status::Working); }, Status::Stopping);
    wait([this] { return isStatus(Status::Idle); });
}

void Thread::stop(TaskId id) {
    {
        Guard g{statusLock};
        if (id != taskId) { return; }
        if (!isStatus(Status::Working)) { return; }
        status = Status::Stopping;
    }
    statusChanged.notify_all();
}
