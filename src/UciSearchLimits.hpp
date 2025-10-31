#ifndef UCI_SEARCH_LIMITS_HPP
#define UCI_SEARCH_LIMITS_HPP

#include <atomic>
#include "Index.hpp"
#include "chrono.hpp"

enum deadline_t { HardDeadline, RootMoveDeadline, IterationDeadline };

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

    TimePoint deadline[3] = {
        TimePoint::max(), // HardDeadline      : tested every QuotaLimit nodes
        TimePoint::max(), // RootMoveDeadline  : tested after every root move search ends
        TimePoint::max()  // IterationDeadline : tested when iteration ends
    };

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
        deadline[HardDeadline]      = TimePoint::max();
        deadline[RootMoveDeadline]  = TimePoint::max();
        deadline[IterationDeadline] = TimePoint::max();
    }

    constexpr TimeInterval average(Side::_t si) const {
        assert (movestogo >= 0);

        if (!movestogo && inc[si] == 0ms) {
            return time[si] / 25; // sudden death
        }

        auto moves = movestogo ? std::min(movestogo, 20) : 20;
        return inc[si] + (time[si]-inc[si])/moves;
    }

    void setSearchDeadline() {
        if (infinite.load(std::memory_order_relaxed)) {
            setNoDeadline();
            return;
        }
        if (movetime > 0ms) {
            setNoDeadline();
            deadline[HardDeadline] = searchStartTime + movetime - moveOverhead;
            return;
        }
        if (time[My] == 0ms) {
            setNoDeadline();
            return;
        }

        // average remaining time per move
        auto myAverage = average(My) + (canPonder ? average(Op) / 2 : 0ms);
        auto hardInterval = std::min(time[My], myAverage * 2);
        auto baseTime = searchStartTime - moveOverhead;

        deadline[HardDeadline]      = baseTime + hardInterval;   // 200% average time
        deadline[RootMoveDeadline]  = baseTime + hardInterval/2; // 100% average time
        deadline[IterationDeadline] = baseTime + hardInterval/4; // 50% average time
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

        if (reached<HardDeadline>()) {
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
    Ply depth{MaxPly};

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
        depth = MaxPly;
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
        reached<HardDeadline>();
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
    bool waitBestmove() {
        return ponder.load(std::memory_order_relaxed) || infinite.load(std::memory_order_relaxed);
    }

    template <deadline_t DeadlineKind>
    bool reached() const {
        if (isStopped()) { return true; }

        bool isDeadline =
            !ponder.load(std::memory_order_relaxed)
            && deadline[DeadlineKind] != TimePoint::max()
            && deadline[DeadlineKind] < ::timeNow();

        if (isDeadline) { stop(); }
        return isDeadline;
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
