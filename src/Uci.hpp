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

class Repetitions;

class UciPosition : public PositionMoves {
    int fullMoveNumber{1}; // number of full moves from the beginning of the game
    Color colorToMove_{White}; //root position side to move color
    ChessVariant chessVariant_{Orthodox}; // castling moves and fen output format, engine accepts any castling input

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;
    ostream& fen(ostream&) const;

public:
    void setStartpos();
    void readFen(istream&);
    void playMoves(istream&, Repetitions&);
    void limitMoves(istream&);

    constexpr Side sideOf(Color::_t color) const { return Side{colorToMove_.is(color) ? My : Op}; }
    constexpr Color colorToMove(Ply ply) const { return Color{ ::distance(colorToMove_, ply) }; }
    constexpr ChessVariant chessVariant() const { return chessVariant_; }
    constexpr void setChessVariant(ChessVariant chessVariant) { chessVariant_ = chessVariant; }

    friend ostream& operator << (ostream& os, const UciPosition& pos) { return pos.fen(os); }
};

class UciOutput;

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
    std::istringstream inputLine;

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
    TimePoint logStartTime;

#ifndef NDEBUG
    bool isDebugOn = true;
    std::string debugPosition;
    std::string debugGo;
#else
    bool isDebugOn = false;
#endif

    std::string evalFileName; // use embedded by default

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // if something left unparsed usually means error
    bool hasMoreInput() { inputLine >> std::ws; return !inputLine.eof(); }

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
    void setEmbeddedEval();
    COLD void loadEvalFile(const std::string&);

    void goPerft();
    void bench();
    void wait();

    // log messages to the logFile named by logFileName
    void log(std::string_view) const;

    ostream& info_fen(ostream&) const;
    UciOutput& info_pv(UciOutput&) const;

    void sendDelayedBestMove() const;
    void info_readyok() const;
    void info_bestmove() const;
    void info_perft_bestmove() const;

public:
    Uci (ostream&);
   ~Uci () { stop(); wait(); }

    // process UCI input commands
    void processInput(istream&);

    // output to cout and (if isDebugOn) to log file
    void output(std::string_view) const;

    // output to cerr and log file
    COLD void error(std::string_view) const;

    // output to log file
    COLD void info(std::string_view) const;

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
