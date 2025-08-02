#include <thread>
#include "Thread.hpp"
#include "TimerManager.hpp"

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
    stdThread = std::thread([this] {
        while (!is(Status::Abort)) {
            signal([this]() { return !is(Status::Ready); }, Status::Ready);
            wait([this] { return is(Status::Run) || is(Status::Abort); });

            if (is(Status::Run)) {
                if (threadTask) {
                    threadTask();
                }
                threadTask = nullptr;
            }
        }
    });
}

Thread::~Thread() {
    {
        Guard g{statusLock};
        status = Status::Abort;
    }
    statusChanged.notify_all();
    if (stdThread.joinable()) { stdThread.join(); }
}

Thread::TaskId Thread::start(Task task) {
    TaskId result;
    {
        Guard g{statusLock};
        if (!is(Status::Ready)) { return TaskId::None; }

        threadTask = std::move(task);
        result = ++taskId;
        status = Status::Run;
    }
    statusChanged.notify_all();
    return result;
}

void Thread::stop() {
    signal([this]() { return is(Status::Run); }, Status::Stop);
    wait([this] { return is(Status::Ready); });
}

void Thread::stop(TaskId id) {
    signal([this, id]() { return id == taskId && is(Status::Run); }, Status::Stop);
}

void Thread::waitStop() {
    wait([this] { return is(Status::Stop) || is(Status::Abort); });
}
