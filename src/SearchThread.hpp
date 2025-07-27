#ifndef SEARCH_THREAD_HPP
#define SEARCH_THREAD_HPP

#include "ThreadControl.hpp"
#include "Node.hpp"

class SearchThread : public ThreadControl {
    std::unique_ptr<Node> node;

    void run() override;

public:
    ~SearchThread(); /* default */

    TaskId start(std::unique_ptr<Node>);
};

#endif
