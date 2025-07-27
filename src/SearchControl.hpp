#ifndef SEARCH_CONTROL_HPP
#define SEARCH_CONTROL_HPP

#include "out.hpp"
#include "Milliseconds.hpp"
#include "NodeCounter.hpp"
#include "PositionFen.hpp"
#include "PvMoves.hpp"
#include "SearchThread.hpp"
#include "Score.hpp"
#include "SpinLock.hpp"
#include "Timer.hpp"
#include "TimePoint.hpp"
#include "Tt.hpp"

class Move;
class UciGoLimit;

class SearchControl {
    SearchControl(const SearchControl&) = delete;
    SearchControl& operator=(const SearchControl&) = delete;

public:
    SearchControl(ostream& o) : out{o} {}

    PositionFen position; // root position between 'position' and 'go' commands

    mutable node_count_t lastInfoNodes = 0; // to avoid two identical output lines in a row
    mutable SpinLock outLock;
    mutable bool isreadyWaiting = false;

    ostream& out; // output stream
    TimePoint fromSearchStart;

    NodeCounter nodeCounter;
    SearchThread searchThread;
    Tt tt;
    Timer timer;
    PvMoves pvMoves;

    void newGame();
    void newSearch();
    void newIteration();

    bool isAborted() const { return nodeCounter.isAborted(); }
    bool isBusy() const { return !searchThread.isIdle(); }
    bool isStopped() const { return searchThread.isStopped(); }
    void stop() { searchThread.stop(); }

    void uciok() const;
    void isready() const;
    void infoPosition() const;
    void go(const UciGoLimit&);
    void setHash(size_t);

    void readyok() const;
    void bestmove() const;
    void infoNewPv(Ply, Score) const;
    void infoIterationEnd(Ply) const;

    void goPerft(Ply depth, bool isDivide = false);
    void perft_depth(Ply, node_count_t) const;
    void perft_currmove(index_t moveCount, const Move& currentMove, node_count_t) const;
    void perft_finish() const;

    NodeControl countNode();

private:
    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }

    ostream& nps(ostream&, node_count_t) const;
    ostream& info_nps(ostream&, node_count_t) const;
};
#endif
