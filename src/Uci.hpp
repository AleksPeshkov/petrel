#ifndef UCI_HPP
#define UCI_HPP

#include <fstream>
#include <mutex>
#include "history.hpp"
#include "io.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciPosition.hpp"
#include "UciSearchLimits.hpp"

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
    UciPosition position_; // result of parsing 'position' command
    UciSearchLimits limits; // result of parsing 'go' command
    Repetitions repetitions;
    Tt tt; // main transposition table

    mutable std::array<UciMove, 6> rootBestMoves;
    mutable PrincipalVariation pv;
    mutable HistoryMoves<4> counterMove;
    mutable HistoryMoves<4> followMove;

private:
    Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

#ifdef ENABLE_ASSERT_LOGGING
    friend void assert_fail(const char*, const char*, unsigned, const char*);
    std::string debugPosition;
    std::string debugGo;
#endif

    // output stream
    ostream& out;
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    mutable std::string bestmove_;
    mutable std::mutex bestmoveMutex;

    // avoid race conditions betweeen Uci output and main search thread output

    std::string logFileName; // no log by default
    mutable std::ofstream logFile;
    mutable std::mutex logMutex;
    const int pid; // system process id (for debug output)

#ifdef NDEBUG
    bool isDebugOn = false;
#else
    bool isDebugOn = true;
#endif

    std::string evalFileName; // use embedded by default

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

//UCI command handlers

    void uciok() const;
    void ucinewgame();
    void position();
    void go();
    void stop();
    void ponderhit();
    void setoption();

    void setHash();
    void setdebug();
    void loadEvalFile(const std::string&);
    void setEmbeddedEval();

    void goPerft();
    void bench();

    ostream& info_fen(ostream&) const;

    void sendDelayedBestMove() const;
    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and if isDebugOn to log file
    void output(std::string_view) const;

    // output to cerr, uci info string, log file
    void error(std::string_view) const;

    // output to uci info string, log file
    void info(std::string_view) const;

    // log messages to the logFile named by logFileName
    void log(std::string_view) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return position_.colorToMove(ply); }

    void bench(std::string& goLimits);

    void newGame();
    void newSearch();
    void newIteration() const;

    void info_pv() const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, UciMove currentMove, node_count_t) const;
};

#endif
