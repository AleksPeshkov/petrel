#ifndef SEARCH_CONTROL_HPP
#define SEARCH_CONTROL_HPP

#include "NodeCounter.hpp"
#include "SearchThread.hpp"
#include "Score.hpp"
#include "PerftTT.hpp"
#include "PvMoves.hpp"
#include "Timer.hpp"

class Move;
class SearchLimit;
class UciSearchInfo;

class SearchControl {
public:
    UciSearchInfo& info; //virtual

protected:
    NodeCounter nodeCounter;
    SearchThread searchThread;

    PerftTT transpositionTable;
    Timer timer;

    PvMoves pvMoves;

    SearchControl (const SearchControl&) = delete;
    SearchControl& operator = (const SearchControl&) = delete;

public:
    SearchControl (UciSearchInfo&);

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
    void go(const SearchLimit&);
    void setHash(size_t);

    const PerftTT& tt() const { return transpositionTable; }
    PerftTT& tt() { return transpositionTable; }

    const Move* getPv(ply_t = 0) const;
    void setPv(ply_t ply, const Move&);

    void readyok() const;
    void bestmove(Score) const;
    void infoDepth(ply_t, Score) const;

    void goPerft(ply_t depth, bool isDivide = false);
    void perft_depth(ply_t, node_count_t) const;
    void perft_currmove(index_t moveCount, const Move& currentMove, node_count_t) const;
    void perft_finish() const;

    NodeControl countNode();

};

#endif
