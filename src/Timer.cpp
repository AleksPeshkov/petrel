#include <thread>
#include "Timer.hpp"
#include "typedefs.hpp"

void TimerThread::run() {
    std::this_thread::sleep_for(timeInterval);
    thread->stop(taskId);
    pool->release(std::move(iterator));
}

// zero time interval means no timer start
void Timer::start(TimeInterval timeInterval, ThreadControl& thread, ThreadControl::TaskId taskId) {
    if (timeInterval == TimeInterval::zero() || taskId == decltype(taskId)::None || !thread.isTask(taskId)) {
        return;
    }

    auto timerIterator = acquire();
    auto& timer = fetch(timerIterator);
    assert (timer.isIdle());

    timer.pool = this;
    timer.iterator = timerIterator;
    timer.thread = &thread;
    timer.taskId = taskId;
    timer.timeInterval = timeInterval;

    timer.start();
}
