#ifndef THREAD_HPP
#define THREAD_HPP

#include <forward_list>
#include <functional>
#include <condition_variable>
#include "chrono.hpp"
#include "SpinLock.hpp"

// Generic object pool pattern
template <class Element, class BasicLockable>
class Pool {
    typedef std::forward_list<Element> List;
    List used;
    List ready;

    BasicLockable listLock;
    typedef std::lock_guard<decltype(listLock)> Guard;

public:
    typedef typename List::iterator Iterator;

    static Element& fetch(const Iterator& iterator) { return *std::next(iterator); }

    Iterator acquire() {
        Guard g{listLock};

        if (ready.empty()) {
            used.emplace_front();
        }
        else {
            used.splice_after(used.before_begin(), ready, ready.before_begin());
        }

        return used.before_begin();
    }

    //return the used element to the ready list
    void release(Iterator&& element) {
        Guard g{listLock};

        if (element != ready.end()) {
            ready.splice_after(ready.before_begin(), used, element);
            element = ready.end();
        }
    }

};

class Thread;
typedef Pool<Thread, SpinLock> ThreadPool;

class Thread {
public:
    enum class TaskId : unsigned { None };
    typedef std::function<void()> Task;

private:
    static ThreadPool timerPool;

    enum class Status {
        Ready, // the thread is ready to get a new task
        Run,  // the thread is busy working on a task with the current taskId
        Stop, // the thread have got the stop signal and is going to be idle soon again
        Abort // the thread have got abort signal
    };

    Status status = Status::Ready;
    SpinLock statusLock;
    std::condition_variable_any statusChanged;

    typedef std::unique_lock<decltype(statusLock)> Guard;

    bool isStatus(Status to) const { return status == to; }

    template <typename Condition> void wait(Condition);
    template <typename Condition> void signal(Condition, Status);

    Task threadTask = nullptr;
    TaskId taskId = TaskId::None;

public:
    Thread();
    ~Thread();

    bool isReady()    const { return isStatus(Status::Ready); }
    bool isStopped() const { return isStatus(Status::Stop) || isStatus(Status::Abort); }

    void stop();
    void stop(TaskId id);

    TaskId start(Task task);
    void startTimer(TaskId id, TimeInterval timeInterval);
};

#endif
