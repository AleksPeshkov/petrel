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
    void isready() const;

    bool isBusy() const { return !searchThread.isIdle(); }
    void stop() { searchThread.stop(); }

    void newGame();
    void newSearch();
    void setHash(size_t);

    void infoPosition() const;
};

#endif
