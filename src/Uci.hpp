#ifndef UCI_HPP
#define UCI_HPP

#include "io.hpp"
#include "PositionFen.hpp"
#include "SearchControl.hpp"
#include "UciSearchInfo.hpp"

using out::ostream;

class Uci {
    PositionFen positionFen; //root position between 'position' and 'go' commands
    UciSearchInfo info; //output with multithread synchronization
    SearchControl searchControl;

    std::istringstream command; //current input command line
    bool next(io::czstring token) { return in::next(command, token); }
    bool nextNothing() { return in::nextNothing(command); }

    //UCI command handlers
    void go();
    void position();
    void setHash();
    void setoption();
    void ucinewgame();
    void goPerft();

public:
    Uci (ostream&);
    void operator() (in::istream&, ostream& = std::cerr);
};

#endif
