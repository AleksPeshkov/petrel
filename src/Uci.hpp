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
    Color colorToMove_{White}; //root position side to move color
    int fullMoveNumber_{1}; // number of full moves from the beginning of the game
    int rootMoves_{0}; // number of legal moves from this position

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

public:
    void setStartpos();
    void readFen(istream&);
    void playMoves(istream&, Repetitions&);
    void limitMoves(istream&);

    int countRootMoves() { rootMoves_ = moves().popcount(); return rootMoves_; }
    UciMove firstRootMove() const;

    constexpr Side sideOf(Color::_t color) const { return Side{colorToMove_.is(color) ? My : Op}; }
    constexpr Color colorToMove(Ply ply) const { return Color{ ::distance(colorToMove_, ply) }; }
    constexpr int fullMoveNumber() const { return fullMoveNumber_; }
    constexpr int rootMoves() const { return rootMoves_; }
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
// used by search:

    UciSearchLimits limits; // result of parsing 'go' command
    Repetitions repetitions;
    Tt tt; // main transposition table

//TODO: per search thread
    mutable std::array<UciMove, 6> rootBestMoves;
    mutable PrincipalVariation pv;
    mutable HistoryMoves<4> counterMove;
    mutable HistoryMoves<4> followMove;

private:
    UciPosition position_; // result of parsing 'position' command
    Thread mainSearchThread;

    ostream& out_; // UCI output stream
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    mutable std::string bestmove_;
    mutable std::mutex bestmoveMutex;

    ChessVariant chessVariant_{Orthodox}; // castling moves and fen output format, engine accepts any castling input

    mutable std::ofstream logFile;
    std::string logFileName; // no log by default
    TimePoint logStartTime;
    const int pid_; // system process id (for debug output)

    std::string evalFileName; // use embedded by default

#ifndef NDEBUG
    bool debugOn_ = true;
    std::string debugPosition;
    std::string debugGo;
#else
    bool debugOn_ = false;
#endif

    // stream buffer for parsing current input line
    io::istringstream inputLine;

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

// UCI commands handlers:

    void uciok() const;
    void ucinewgame();
    void position();
    void go();
    void stop();
    void ponderhit();
    void setoption();

    void setHash();
    void setDebugOn();
    void loadEvalFile(const std::string&);
    void setEmbeddedEval();

    void goPerft();
    void bench();

    void log(io::char_type tag, std::string_view) const; // log messages to the file logFileName
    void _log(io::char_type tag, std::string_view, bool flush = true) const; // write into logFile without mutex and logFileName check

    void sendDelayedBestMove() const;
    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to cout and (if debugOn_) to log file
    void output(std::string_view, bool flush = true) const;

    // output to cerr and log file
    void error(std::string_view) const;

    // output to log file
    void info(std::string_view) const;

    constexpr ChessVariant chessVariant() const { return chessVariant_; }
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
