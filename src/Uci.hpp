#ifndef UCI_HPP
#define UCI_HPP

#include <atomic>
#include <mutex>
#include "io.hpp"
#include "Thread.hpp"
#include "UciRoot.hpp"

class Uci {
    UciRoot root;
    mutable Thread searchThread;

    mutable std::mutex outLock;
    ostream& out; // output stream
    mutable std::atomic<bool> isreadyWaiting = false;

    std::istringstream command; //current input command line

    mutable node_count_t lastInfoNodes = 0; // to avoid printing identical nps info lines in a row

    bool next(io::czstring token) { return in::next(command, token); }
    bool nextNothing() { return in::nextNothing(command); }

    //UCI command handlers
    void go();

    void position();
    void setHash();
    void setoption();
    void ucinewgame();

    void goPerft();

    void uciok() const;
    void readyok() const;
    void infoPosition() const;

    bool isReady() const { return searchThread.isReady(); }

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;

public:
    Uci (io::ostream& o): root{*this}, out{o} { ucinewgame(); }
    void operator() (io::istream&, io::ostream& = std::cerr);

    ChessVariant chessVariant() const { return root.chessVariant(); }

    // called from NodeCounter::refreshQuota()
    bool isStopped() const { return searchThread.isStopped(); }
    void infoNpsReadyok() const;

    void bestmove() const;
    void infoNewPv(Ply, Score) const;
    void infoIterationEnd(Ply) const;

    void perft_depth(Ply, node_count_t) const;
    void perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t) const;
    void perft_finish() const;

    void waitStop() const { searchThread.waitStop(); }
};

#endif
