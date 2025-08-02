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
    std::thread([this] {
        for (;;) {
            wait([this] { return isStatus(Status::Run) || isStatus(Status::Abort); });
            {
                Guard g{statusLock};
                if (isStatus(Status::Abort)) { break; }
            }

            if (threadTask) {
                threadTask();
            }
            threadTask = nullptr;

            {
                Guard g{statusLock};
                if (isStatus(Status::Abort)) { break; }
            }
            signal([this]() { return !isStatus(Status::Ready); }, Status::Ready);
        }
    }).detach();
}

Thread::~Thread() {
    {
        Guard g{statusLock};
        status = Status::Abort;
    }
    statusChanged.notify_all();
}

Thread::TaskId Thread::start(Task task) {
    TaskId result;
    {
        Guard g{statusLock};
        if (!isStatus(Status::Ready)) { return TaskId::None; }

        threadTask = std::move(task);
        result = ++taskId;
        status = Status::Run;
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
    signal([this]() { return isStatus(Status::Run); }, Status::Stop);
    wait([this] { return isStatus(Status::Ready); });
}

void Thread::stop(TaskId id) {
    signal([this, id]() { return id == taskId && isStatus(Status::Run); }, Status::Stop);
}
