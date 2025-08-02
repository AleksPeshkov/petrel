#ifndef UCI_GO_ROOT_HPP
#define UCI_GO_ROOT_HPP

#include "SearchRoot.hpp"

class UciGoLimit;

class UciGoRoot : protected SearchRoot {
public:
    using SearchRoot::SearchRoot;
    using SearchRoot::position;
    using SearchRoot::repetition;

    void go(const UciGoLimit&);
    void goPerft(Ply depth, bool isDivide = false);

    void uciok() const;

    // inform from input search directly or delegate to search thread
    void readyok() const;

    bool isReady() const { return searchThread.isReady(); }
    void stop() { searchThread.stop(); }

    void newGame();
    void newSearch();
    void setHash(size_t);

    void infoPosition() const;
};

#endif
