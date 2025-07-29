#include <cassert>
#include "ThreadRun.hpp"

ThreadRun::~ThreadRun() = default;

ThreadControl::TaskId ThreadRun::start(std::unique_ptr<Runnable> r) {
    assert (isIdle());
    assert (!runnable);

    runnable = std::move(r);
    return ThreadControl::start();
}

void ThreadRun::run() {
    assert (isRunning());
    assert (runnable);

    runnable->run();
    runnable.release();
}
