#ifndef SEARCH_CONTROL_HPP
#define SEARCH_CONTROL_HPP

#include "ThreadControl.hpp"

// runnable interface
class Runnable {
public:
    virtual void run() = 0;
    virtual ~Runnable() {}
};

class ThreadRun : public ThreadControl {
    std::unique_ptr<Runnable> runnable;

    void run() override;

public:
    ~ThreadRun(); /* default */

    TaskId start(std::unique_ptr<Runnable>);
};

#endif
