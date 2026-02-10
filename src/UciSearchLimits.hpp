#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include <atomic>
#include "typedefs.hpp"
#include "chrono.hpp"
#include "Thread.hpp"

class UciSearchLimits {
    constexpr static int QuotaLimit = 200; // < 0.05ms

    mutable node_count_t nodes = 0; // (0 <= nodes && nodes <= nodesLimit)
    mutable node_count_t nodesLimit = NodeCountMax; // search limit

    //number of remaining nodes before slow checking for search stop
    mutable int nodesQuota = 0; // (0 <= nodesQuota && nodesQuota <= QuotaLimit)

    mutable std::atomic_bool stop_{false};
    mutable std::atomic_bool infinite{false};
    mutable std::atomic_bool ponder{false};

    TimePoint searchStartTime;
    TimePoint hardDeadline = TimePoint::max(); // tested every QuotaLimit nodes
    TimePoint rootMoveDeadline = TimePoint::max(); // tested before next root move search
    TimePoint iterationDeadline = TimePoint::max(); // tested when iteration ends

    Side::arrayOf<TimeInterval> time = {{ 0ms, 0ms }};
    Side::arrayOf<TimeInterval> inc = {{ 0ms, 0ms }};
    TimeInterval movetime = 0ms;

    int movestogo = 0;
    int mate = 0;

    constexpr void assertNodesOk() const {
        assert (0 <= nodesQuota);
        assert (nodesQuota < QuotaLimit);
        //assert (0 <= nodes);
        assert (nodes <= nodesLimit);
        assert (static_cast<decltype(nodesLimit)>(nodesQuota) <= nodes);
    }

    constexpr void setNoDeadline() {
        hardDeadline = TimePoint::max();
        iterationDeadline = TimePoint::max();
        rootMoveDeadline = TimePoint::max();
    }

    constexpr TimeInterval average(Side my) const {
        auto moves = (movestogo > 0) ? movestogo : 30;
        return (time[my] - inc[my]) / moves + inc[my];
    }

    void setSearchDeadline() {
        if (infinite.load(std::memory_order_relaxed)) {
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
        auto myAverage = average(My) + (canPonder ? average(Op) / 3 : 0ms);
        auto hardInterval = std::min(time[My], myAverage * 3 / 2);
        auto baseTime = searchStartTime - moveOverhead;

        hardDeadline = baseTime + hardInterval; // 150% average time
        rootMoveDeadline = baseTime + (hardInterval * 2 / 3); // 100% average time
        iterationDeadline = baseTime + (hardInterval / 3); // 50% average time
    }

    ReturnStatus refreshQuota() const {
        assertNodesOk();
        nodes -= nodesQuota;

        auto nodesRemaining = nodesLimit - nodes;
        if (nodesRemaining >= QuotaLimit) {
            nodesQuota = QuotaLimit;
        }
        else {
            nodesQuota = static_cast<decltype(nodesQuota)>(nodesRemaining);
            if (nodesQuota == 0) {
                assertNodesOk();
                return ReturnStatus::Stop;
            }
        }

        if (isHardDeadline()) {
            nodesLimit = nodes;
            nodesQuota = 0;

            assertNodesOk();
            return ReturnStatus::Stop;
        }

        assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
        nodes += nodesQuota;
        --nodesQuota; //count current node

        assertNodesOk();
        return ReturnStatus::Continue;
    }

public:
    Ply depth = {MaxPly};

    bool canPonder{false};

    TimeInterval moveOverhead = 100us;

    // clear all limits except canPonder and moveOverhead
    void clear() {
        nodes = 0;
        nodesLimit = NodeCountMax;
        nodesQuota = 0;
        searchStartTime = timeNow();
        setNoDeadline();
        movetime = 0ms;
        time = {{ 0ms, 0ms }};
        inc = {{ 0ms, 0ms }};
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
            else if (io::consume(in, "nodes"))    { in >> nodesLimit; }
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

    ostream& options(ostream& out) const {
        out << "option name Move Overhead type spin min 0 max 10000 default " << moveOverhead << '\n';
        out << "option name Ponder type check default " << (canPonder ? "true" : "false") << '\n';
        return out;
    }

    void ponderhit() {
        ponder.store(false, std::memory_order_relaxed);
        isHardDeadline();
    }

    void stop() const {
        infinite.store(false, std::memory_order_relaxed);
        ponder.store(false, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_release);
    }

    bool isStopped() const {
        return stop_.load(std::memory_order_acquire);
    }

    // ponder || infinite
    bool shouldDelayBestmove() {
        return ponder.load(std::memory_order_relaxed) || infinite.load(std::memory_order_relaxed);
    }

    // !ponder && hardDeadline
    bool isHardDeadline() const {
        if (isStopped()) { return true; }

        auto deadline =
            !ponder.load(std::memory_order_relaxed)
            && hardDeadline != TimePoint::max()
            && hardDeadline < ::timeNow();

        if (deadline) { stop(); }
        return deadline;
    }

    // !ponder && iterationDeadline
    bool isIterationDeadline() const {
        if (isStopped()) { return true; }

        auto deadline =
            !ponder.load(std::memory_order_relaxed)
            && iterationDeadline != TimePoint::max()
            && iterationDeadline < ::timeNow();

        if (deadline) { stop(); }
        return deadline;
    }

    // !ponder && rootMoveDeadline
    bool isRootMoveDeadline() const {
        if (isStopped()) { return true; }

        auto deadline =
            !ponder.load(std::memory_order_relaxed)
            && rootMoveDeadline != TimePoint::max()
            && rootMoveDeadline < ::timeNow();

        if (deadline) { stop(); }
        return deadline;
    }

    TimeInterval elapsedSinceStart() const {
        return ::elapsedSince(searchStartTime);
    }

    /// exact number of visited nodes
    constexpr node_count_t getNodes() const {
        assertNodesOk();
        return nodes - nodesQuota;
    }

    ReturnStatus countNode() const {
        assertNodesOk();

        if (nodesQuota == 0 || isStopped()) {
            return refreshQuota();
        }

        assert (nodesQuota > 0);
        --nodesQuota;

        assertNodesOk();
        return ReturnStatus::Continue;
    }
};

#endif
