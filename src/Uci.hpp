#ifndef UCI_HPP
#define UCI_HPP

#include "io.hpp"
#include "UciGoRoot.hpp"

using out::ostream;

class Uci {
    UciGoRoot root;

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
    Uci (ostream& out): root{out} { ucinewgame(); }
    void operator() (in::istream&, ostream& = std::cerr);
};

#endif
