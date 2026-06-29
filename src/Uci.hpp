#ifndef UCI_HPP
#define UCI_HPP

#include <fstream>
#include <mutex>
#include "history.hpp"
#include "io.hpp"
#include "PositionMoves.hpp"
#include "search.hpp"
#include "SearchLimits.hpp"
#include "Thread.hpp"
#include "Tt.hpp"

class UciPosition : public PositionMoves {
    Color colorToMove_{White}; //root position side to move color
    int fullMoveNumber_{1}; // number of played full moves from the beginning of the game

    istream& readBoard(istream&);
    istream& readCastling(istream&);
    istream& readEnPassant(istream&);
    istream& readMove(istream&, Square&, Square&) const;

public:
    void readFen(istream&);
    void playMoves(istream&, Repetitions&);

    Move firstRootMove() const;

    constexpr Side sideOf(Color::_t color) const { return Side{colorToMove_.is(color) ? My : Op}; }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return Color{ ::distance(colorToMove_, ply) }; }
    constexpr auto fullMoveNumber(Ply ply = 0_ply) const { return fullMoveNumber_ + (+ply + +colorToMove_) / 2; }
};

struct UciLimits {
    static constexpr TimeInterval MoveOverheadDefault{1500us};

    array<TimeInterval, Color> time{ 0ms, 0ms }; // go wtime|btime
    array<TimeInterval, Color> inc{ 0ms, 0ms }; // go winc|binc
    TimeInterval movetime{0ms}; // go movetime
    TimeInterval moveOverhead{MoveOverheadDefault}; // option Move Overhead

    node_count_t nodes{NodeCountMax}; // go nodes
    int movestogo{0}; // go movestogo
    Ply depth{MaxPly}; // go depth

    bool ponder{false}; // go ponder
    bool infinite{false}; // go infinite
    bool canPonder{false}; // option Ponder
    bool isNewGame{false}; // set by Uci::newGame(), reset by Uci::go()

    void readGo(istream&);
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
    UciPosition position_; // result of parsing 'position' command
    UciLimits go_; // state after parsing 'go' and `setoption` commands
    Thread mainSearchThread;

    std::istringstream inputLine; // stream buffer for parsing current input line

    mutable std::mutex outMutex; // for both out_ and logFile
    ostream& out_; // UCI output stream
    mutable std::ofstream logFile;

    std::string bestmove_; // in infinite or ponder mode we cannot send bestmove immediately
    std::mutex bestmoveMutex;
    bool infinite_{false}; // should delay bestmove output

    mutable node_count_t lastInfoNodes_{0}; // avoid output duplicate 'info nps'
    mutable TimePoint lastInfoTime_{}; // for instantaneous nps

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
    SearchLimits limits; // inited from UciLimits and UciPosition
    Repetitions repetitions;
    Tt tt; // main transposition table

//TODO: per search thread
    array<Node, Ply> searchStack;
    ContMoves<4> contMoves;
    CheckMoves counterCheck;
    PrincipalVariation pv;
    std::array<Move, 6> rootBestMoves;

private:
// input members and methods:

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // discovered parsing or other error
    bool leftUnparsedInput() { return !(inputLine >> std::ws).eof(); }

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
    void readStartPos();
    void setPositionMoves();
    void setHash();
    void setDebugOn();
    void setEmbeddedEval();
    COLD void loadEvalFile(const std::string&);

    void swapBestMove(std::string&);
    void outputBestMove();

    void info_readyok() const;
    void info_bestmove();
    void info_perft_bestmove() const;

    constexpr bool hasNewNodes() const { return lastInfoNodes_ != limits.getNodes(); }
    ostream& average_nps(ostream&) const;
    ostream& instant_nps(ostream&) const;
    ostream& info_pv(ostream&) const;

    void info(std::string_view) const; // output to log file
    void log(io::char_type tag, std::string_view) const; // log messages to the logFile named by logFileName
    void _log(io::char_type tag, std::string_view, bool flush = true) const; // write into logFile without mutex and logFileName check

public:
    Uci (ostream&);
   ~Uci () { stop(); wait(); }
    void processInput(istream&); // process UCI input commands
    void bench(std::string_view goLimits);

    void output(std::string_view, bool flush = true) const; // output to cout and (if debugOn_) to log file
    COLD void error(std::string_view prefix, std::string_view suffix = {}) const; // output to cerr and log file

    constexpr ChessVariant chessVariant() const { return chessVariant_; }
    constexpr Color colorToMove(Ply ply = 0_ply) const { return position_.colorToMove(ply); }
    constexpr auto fullMoveNumber(Ply ply = 0_ply) const { return position_.fullMoveNumber(ply); }
    constexpr const auto& moves() const { return position_.moves(); }

    void info_pv() const;
    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, Move currentMove, node_count_t) const;

    void move(ostream&, Move, Ply = 0_ply) const;
    void fen(ostream&, const Position&, Ply = 0_ply) const;
};

extern Uci The_uci;

#endif
