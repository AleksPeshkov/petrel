#ifndef UCI_HPP
#define UCI_HPP

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
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
    typedef std::lock_guard<decltype(outLock)> Guard;

    // used to respond to "isready" command with "info nps"
    mutable std::atomic<bool> readyokWaiting = false;

    // to avoid printing identical nps info lines in a row
    mutable node_count_t lastInfoNodes = 0;

    bool canPonder = false;

    std::string logFileName;
    mutable std::ofstream logFile;

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


    bool isReady() const { return mainSearchThread.isReady(); }

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

    void setDeadline(TimeInterval);

    void bestmove() const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(const std::string&) const;

    // log messages to the logFile named by logFileName
    void log(const std::string&) const;

    ChessVariant chessVariant() const { return root.chessVariant(); }

    void info_pv(Ply, Score) const;
    void info_iteration(Ply) const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t) const;
    void info_perft_bestmove() const;

    void waitStop() const { mainSearchThread.waitStop(); }

    // called from NodeCounter::refreshQuota()
    bool isStopped() const { return mainSearchThread.isStopped(); }
    void search_readyok() const;
};

#endif
