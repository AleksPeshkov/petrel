#ifndef UCI_SEARCH_INFO_HPP
#define UCI_SEARCH_INFO_HPP

#include "out.hpp"
#include "typedefs.hpp"
#include "Score.hpp"
#include "SpinLock.hpp"
#include "TimePoint.hpp"

class HashInfo;
class Move;
class PerftTT;
class PositionFen;

using out::ostream;

class UciSearchInfo {
public:
    ostream& out; //output stream
    const PositionFen& positionFen; //current position
    mutable node_count_t lastInfoNodes = 0;

    TimePoint fromSearchStart;

    mutable SpinLock outLock;
    mutable bool isreadyWaiting = false; //set when got 'isready' command while thinking

    ostream& nps(ostream&, node_count_t, const PerftTT&) const;
    ostream& info_nps(ostream&, node_count_t, const PerftTT&) const;

    UciSearchInfo (ostream&, const PositionFen&);

    //called from Uci
    void isready(bool) const;
    void position() const;
    void uciok(const HashInfo&) const;

    //called from Search
    void clear();

    void readyok(node_count_t, const PerftTT&) const;
    ostream& write(ostream&, const Move& move, ply_t = 0) const;
    ostream& write(ostream&, const Move[], ply_t = 0) const;

    void bestmove(const Move[], Score, node_count_t, const PerftTT&) const;
    void report_depth(ply_t, const Move[], Score, node_count_t, const PerftTT&) const;

    void perft_depth(ply_t, node_count_t, node_count_t, const PerftTT&) const;
    void perft_currmove(const Move&, index_t, node_count_t, node_count_t, const PerftTT&) const;
    void perft_finish(node_count_t, const PerftTT& tt) const;

};

#endif
