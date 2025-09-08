#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include <atomic>
#include "typedefs.hpp"
#include "chrono.hpp"
#include "Thread.hpp"

class UciSearchLimits {
    std::atomic_bool stop_{false};
    std::atomic_bool infinite{false};
    std::atomic_bool ponder{false};

    TimePoint searchStartTime;
    TimePoint hardDeadline = TimePoint::max(); // tested every 100 nodes
    TimePoint iterationDeadline = TimePoint::max(); // tested when iteration ends
    TimePoint updatePvDeadline = TimePoint::max(); // tested when pv updates at root

    Side::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Side::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};
    TimeInterval movetime = 0ms;

    int movestogo = 0;
    int mate = 0;

    constexpr void setNoDeadline() {
        hardDeadline = TimePoint::max();
        iterationDeadline = TimePoint::max();
        updatePvDeadline = TimePoint::max();
    }

    constexpr TimeInterval average(Side my) const {
        auto moves = (movestogo > 0) ? movestogo : 30;
        return (time[my] + (moves-1)*inc[my]) / moves;
    }

public:
    Ply depth = {MaxPly};
    node_count_t nodes = NodeCountMax;

    bool canPonder{false};

    TimeInterval moveOverhead = 100us;

    // clear all limits except canPonder and moveOverhead
    void clear() {
        searchStartTime = timeNow();
        setNoDeadline();
        movetime = 0ms;
        time = {{ 0ms, 0ms }};
        inc = {{ 0ms, 0ms }};
        nodes = NodeCountMax;
        depth = {MaxPly};
        movestogo = 0;
        mate = 0;
        stop_.store(false, std::memory_order_relaxed);
        ponder.store(false, std::memory_order_relaxed);
        infinite.store(false, std::memory_order_relaxed);
    }

    istream& go(istream& in, Side white) {
        clear();

        while (in >> std::ws, !in.eof()) {
            if      (io::consume(in, "depth"))    { in >> depth; }
            else if (io::consume(in, "nodes"))    { in >> nodes; }
            else if (io::consume(in, "movetime")) { in >> movetime; }
            else if (io::consume(in, "wtime"))    { in >> time[white]; }
            else if (io::consume(in, "btime"))    { in >> time[~white]; }
            else if (io::consume(in, "winc"))     { in >> inc[white]; }
            else if (io::consume(in, "binc"))     { in >> inc[~white]; }
            else if (io::consume(in, "movestogo")){ in >> movestogo; }
            else if (io::consume(in, "mate"))     { in >> mate; } // TODO: implement mate in n moves
            else if (io::consume(in, "ponder"))   { ponder.store(true, std::memory_order_relaxed); }
            else if (io::consume(in, "infinite")) { infinite.store(true, std::memory_order_relaxed); }
            else { break; }
        }

        setSearchDeadline();
        return in;
    }

    void setSearchDeadline(TimeInterval ponderElapsed = 0ms) {
        if (waitBestmove()) {
            setNoDeadline();
            return;
        }

        if (movetime > 0ms) {
            setNoDeadline();
            hardDeadline = searchStartTime + movetime - moveOverhead;
            return;
        }

        if (time[My] == 0ms) {
            setNoDeadline();
            return;
        }

        // average remaining time per move
        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);

        auto hardInterval = std::max<TimeInterval>(0ms, std::min(time[My], myAverage * 3 / 2) - moveOverhead); // 150% average time

        if (hardInterval > 100us || ponderElapsed > 100us) {
            // normal time management
            hardDeadline = searchStartTime + hardInterval;
            iterationDeadline = searchStartTime + (hardInterval / 3); // 50% average time
            updatePvDeadline = searchStartTime + (hardInterval * 2 / 3); // 100% average time
            return;
        }

        io::log("#hardInterval too small");

        // almost no time left
        // return hash move or finish iteration 1 to get a reasonable best move
        setNoDeadline();
        iterationDeadline = searchStartTime;
    }

    constexpr void ponderhit() {
        if (!ponder.load(std::memory_order_relaxed)) { return; } // ignore ponderhit if not pondering

        ponder.store(false, std::memory_order_relaxed);
        auto elapsed = ::elapsedSince(searchStartTime);
        time[Op] -= std::min(elapsed, time[Op]);
        setSearchDeadline(elapsed);
    }

    constexpr bool isStopped() const { return stop_.load(std::memory_order_acquire); }

    void stop() {
        infinite.store(false, std::memory_order_relaxed);
        ponder.store(false, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_release);
    }

    // ponder || infinite
    bool waitBestmove() { return ponder.load(std::memory_order_relaxed) || infinite.load(std::memory_order_relaxed); }

    bool hardDeadlineReached() {
        auto deadline = hardDeadline != TimePoint::max() && hardDeadline < ::timeNow();
        if (deadline) { stop(); }
        return deadline;
    }

    bool iterationDeadlineReached() {
        auto deadline = iterationDeadline != TimePoint::max() && iterationDeadline < ::timeNow();
        if (deadline) { stop(); }
        return deadline;
    }

    bool updatePvDeadlineReached() {
        auto deadline = updatePvDeadline != TimePoint::max() && updatePvDeadline < ::timeNow();
        if (deadline) { stop(); }
        return deadline;
    }

    constexpr TimeInterval elapsedSinceStart() const { return ::elapsedSince(searchStartTime); }

    friend ostream& operator << (ostream& out, const UciSearchLimits& limits) {
        out << "option name Move Overhead type spin min 0 max 10000 default " << limits.moveOverhead << '\n';
        //out << "option name Ponder type check default " << (limits.canPonder ? "true" : "false") << '\n';
        return out;
    }
};

#endif
