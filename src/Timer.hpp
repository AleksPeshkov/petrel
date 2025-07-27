#ifndef TIMER_HPP
#define TIMER_HPP

#include <forward_list>
#include <mutex>
#include "chrono.hpp"
#include "SpinLock.hpp"
#include "ThreadControl.hpp"

/**
 * Generic object pool pattern
 **/
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

class TimerThread;
typedef Pool<TimerThread, SpinLock> TimerPool;

class TimerThread : private ThreadControl {
    friend class Timer;

    TimeInterval timeInterval;
    ThreadControl* thread;
    TimerPool* pool;
    TimerPool::Iterator iterator;
    TaskId taskId;

    void run() override;
};

class Timer : private TimerPool {
public:
    void start(TimeInterval, ThreadControl&, ThreadControl::TaskId);
};

#endif
