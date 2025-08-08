#ifndef UCI_HPP
#define UCI_HPP

#include <atomic>
#include <mutex>
#include "io.hpp"
#include "Thread.hpp"
#include "UciRoot.hpp"

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
    UciRoot root;
    mutable Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

    // output stream
    ostream& out;

    // avoid race conditions betweeen Uci output and main search thread output
    mutable std::mutex outLock;

    // used to respond to "isready" command with "info nps"
    mutable std::atomic<bool> isreadyWaiting = false;

    // to avoid printing identical nps info lines in a row
    mutable node_count_t lastInfoNodes = 0;

    bool canPonder = false;

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

    //UCI command handlers
    void go();
    void position();
    void setoption();
    void ucinewgame();
    void ponderhit();
    void uciok() const;
    void readyok() const;

    void setHash();
    void goPerft();

    void infoPosition() const;

    bool isReady() const { return mainSearchThread.isReady(); }

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;

    void setDeadline(TimeInterval);

    void bestmove() const;

public:
    Uci (io::ostream& o): root{*this}, out{o} { ucinewgame(); }

    // process UCI input commands
    void processCommands(io::istream&, io::ostream& = std::cerr);

    ChessVariant chessVariant() const { return root.chessVariant(); }

    void info_pv(Ply, Score) const;
    void info_iteration(Ply) const;

    void perft_depth(Ply, node_count_t) const;
    void perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t) const;
    void perft_finish() const;

    void waitStop() const { mainSearchThread.waitStop(); }

    // called from NodeCounter::refreshQuota()
    bool isStopped() const { return mainSearchThread.isStopped(); }
    void info_nps_readyok() const;
};

#endif
