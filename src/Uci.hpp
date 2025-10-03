#ifndef UCI_HPP
#define UCI_HPP

#include <fstream>
#include <mutex>
#include "io.hpp"
#include "Repetitions.hpp"
#include "Thread.hpp"
#include "Tt.hpp"
#include "UciPosition.hpp"
#include "UciMove.hpp"
#include "UciSearchLimits.hpp"

class HistoryMoves {
    using _t = Side::arrayOf< PieceType::arrayOf<Square::arrayOf<Move>> >;
    _t v;
public:
    void clear() { std::memset(&v, 0, sizeof(v)); }
    const Move& operator() (Color c, PieceType ty, Square sq) const { return v[c][ty][sq]; }
    void set(Color c, PieceType ty, Square sq, const Move& move) { v[c][ty][sq] = move; }
};

// triangular array
class CACHE_ALIGN PvMoves {
    Ply::arrayOf< Ply::arrayOf<UciMove> > pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void set(Ply ply, UciMove move) {
        pv[ply][0] = move;
        auto target = &pv[ply][1];
        auto source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = {};
    }

    operator const UciMove* () const { return &pv[0][0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << &pvMoves.pv[0][0];
    }
};

/// Handling input and output of UCI (Universal Chess Interface)
class Uci {
public:
    UciPosition position_; // result of parsing 'position' command
    UciSearchLimits limits; // result of parsing 'go' command

    mutable Tt tt;
    Repetitions repetitions;
    mutable PvMoves pvMoves;
    mutable Score pvScore = NoScore;
    mutable HistoryMoves counterMove;

private:
    Thread mainSearchThread;

    // stream buffer for parsing current input line
    io::istringstream inputLine;

    // output stream
    ostream& out;
    mutable std::mutex outMutex;

    // if we are in infinite or ponder mode, we cannot send bestmove immediately
    std::string bestmove_;
    std::mutex bestmoveMutex;

    // avoid race conditions betweeen Uci output and main search thread output

    // avoid printing identical 'info nps' lines in a row
    mutable node_count_t lastInfoNodes = 0;

    std::string logFileName;
    mutable std::ofstream logFile;
    mutable std::mutex logMutex;

    // try to consume the given token from the inputLine
    bool consume(io::czstring token) { return io::consume(inputLine, token); }

    // something left unparsed
    bool hasMoreInput() { return io::hasMore(inputLine); }

    //UCI command handlers

    void uciok() const;
    void setoption();
    void setHash();
    void ucinewgame();
    void position();

    void readyok() const;
    void go();
    void stop();
    void ponderhit();
    void bestmove();

    void goPerft();
    void info_perft_bestmove();

    ostream& nps(ostream&) const;
    ostream& info_nps(ostream&) const;
    ostream& info_fen(ostream&) const;

public:
    Uci (ostream&);

    // process UCI input commands
    void processInput(istream&);

    // output to out stream and to log file
    void output(const std::string&) const;

    // log messages to the logFile named by logFileName
    void log(const std::string&) const;

    constexpr ChessVariant chessVariant() const { return position_.chessVariant(); }
    constexpr Color colorToMove() const { return position_.colorToMove(); }

    void newGame() {
        tt.newGame();
        counterMove.clear();
        newSearch();
    }

    void newSearch() {
        tt.newSearch();
        pvMoves.clear();
        pvScore = NoScore;
    }

    void newIteration() const {
        tt.newIteration();
    }

    void setHash(size_t bytes) { tt.setSize(bytes); }
    void info_pv(Ply) const;
    void info_iteration(Ply) const;

    void info_perft_depth(Ply, node_count_t) const;
    void info_perft_currmove(int moveCount, const UciMove& currentMove, node_count_t) const;
};

#endif
