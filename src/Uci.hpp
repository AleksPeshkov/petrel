#ifndef UCI_HPP
#define UCI_HPP

#include <fstream>
#include <mutex>
#include "history.hpp"
#include "io.hpp"
#include "PositionMoves.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciSearchLimits.hpp"

class UciPosition : public PositionMoves {
    int fullMoveNumber_{1}; // number of full moves from the beginning of the game
    Color colorToMove_{White}; //root position side to move color

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

public:
    void setStartpos();
    void readFen(istream&);
    void playMoves(istream&, Repetitions&);
    void limitMoves(istream&);

    constexpr Side sideOf(Color::_t color) const { return Side{colorToMove_.is(color) ? My : Op}; }
    constexpr Color colorToMove(Ply ply) const { return Color{ ::distance(colorToMove_, ply) }; }
    constexpr int fullMoveNumber() const { return fullMoveNumber_; }
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
    UciPosition position_; // result of parsing 'position' command
    Thread mainSearchThread;

    std::istringstream inputLine; // stream buffer for parsing current input line

    mutable std::mutex outMutex; // for both out_ and logFile
    ostream& out_; // UCI output stream
    mutable std::ofstream logFile;

    std::string bestmove_; // in infinite or ponder mode we cannot send bestmove immediately
    std::mutex bestmoveMutex;

// log pretty printing:

    TimePoint logStartTime; // base time for logging
    const int pid_; // system process id (for debug output)

#ifndef NDEBUG
    bool debugOn_ = true;
    std::string debugPosition;
    std::string debugGo;
#else
    bool debugOn_ = false;
#endif

// UCI options:

    ChessVariant chessVariant_{Orthodox}; // castling moves and fen output format, engine accepts any castling input
    std::string logFileName; // no log by default
    std::string evalFileName; // use embedded by default

public: // used by search:
    UciSearchLimits limits; // result of parsing 'go' command
    Repetitions repetitions;
    Tt tt; // main transposition table

//TODO: per search thread
    mutable HistoryMoves<4> counterMove;
    mutable HistoryMoves<4> followMove;
    mutable PrincipalVariation pv;
    mutable std::array<UciMove, 6> rootBestMoves;

private:
// input members and methods:

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // if something left unparsed usually means error
    bool hasMoreInput() { inputLine >> std::ws; return !inputLine.eof(); }

// UCI commands handlers:

    void uciok() const;
    void setoption();
    void ucinewgame();
    void position();
    void go();
    void stop();
    void ponderhit();
    void wait();
    void bench();
    void perft();

    void newGame();
    void newSearch();
    void setHash();
    void setDebugOn();
    void setEmbeddedEval();
    COLD void loadEvalFile(const std::string&);

    void swapBestMove(std::string&);
    void outputBestMove();

    void info_readyok() const;
    void info_bestmove();
    void info_perft_bestmove() const;

    void info(std::string_view) const; // output to log file
    void log(io::char_type tag, std::string_view) const; // log messages to the logFile named by logFileName
    void _log(io::char_type tag, std::string_view, bool flush = true) const; // write into logFile without mutex and logFileName check

public:
    Uci (ostream&);
   ~Uci () { stop(); wait(); }
    void processInput(istream&); // process UCI input commands
    void bench(std::string& goLimits);

    void output(std::string_view, bool flush = true) const; // output to cout and (if debugOn_) to log file
    COLD void error(std::string_view) const; // output to cerr and log file

    constexpr ChessVariant chessVariant() const { return chessVariant_; }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return position_.colorToMove(ply); }

    void info_pv() const;
    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, UciMove currentMove, node_count_t) const;
};

#endif
