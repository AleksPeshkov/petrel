#include "SearchRoot.hpp"

namespace {
    io::ostream& operator << (io::ostream& out, TimeInterval& timeInterval) {
        using namespace std::chrono_literals;
        if (timeInterval < 1ms) { return out; }

        return out << " time " << duration_cast<Msecs>(timeInterval).count();
    }

    template <typename T>
    static T mebi(T bytes) { return bytes / (1024 * 1024); }

    template <typename T>
    static constexpr T permil(T n, T m) { return (n * 1000) / m; }
}

#define OUTPUT(ob) OutputBuffer<decltype(outLock)> ob(out, outLock)

void SearchRoot::newIteration() {
    tt.newIteration();
}

ReturnStatus SearchRoot::countNode() {
    return nodeCounter.count(*this);
}

void SearchRoot::readyok() const {
    if (isreadyWaiting) {
        std::ostringstream ob;
        info_nps(ob);
        ob << "readyok\n";

        if (outLock.try_lock()) {
            if (isreadyWaiting) {
                isreadyWaiting = false;
                out << ob.str() << std::flush;
            }
            outLock.unlock();
        }
    }
}

void SearchRoot::bestmove() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove " << pvMoves[0] << '\n';
}

void SearchRoot::infoIterationEnd(Ply draft) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << '\n';
}

void SearchRoot::infoNewPv(Ply draft, Score score) const {
    OUTPUT(ob);
    ob << "info depth " << draft; nps(ob) << score << " pv" << pvMoves << '\n';
}

void SearchRoot::perft_depth(Ply draft, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info depth " << draft << " perft " << perft; nps(ob) << '\n';
}

void SearchRoot::perft_currmove(index_t moveCount, const UciMove& currentMove, node_count_t perft) const {
    OUTPUT(ob);
    ob << "info currmovenumber " << moveCount << " currmove " << currentMove << " perft " << perft;
    nps(ob) << '\n';
}

void SearchRoot::perft_finish() const {
    OUTPUT(ob);
    info_nps(ob);
    ob << "bestmove 0000\n";
}

ostream& SearchRoot::nps(ostream& o) const {
    if (lastInfoNodes == nodeCounter) {
        return o;
    }
    lastInfoNodes = nodeCounter;

    auto timeInterval = fromSearchStart.getDuration();

    o << " nodes " << lastInfoNodes << timeInterval << " nps " << ::nps(lastInfoNodes, timeInterval);

    if (tt.reads > 0) {
        o << " hwrites " << tt.writes;
        o << " hhits " << tt.hits;
        o << " hreads " << tt.reads;
        o << " hhitratio " << permil(tt.hits, tt.reads);
    }
    return o;
}

ostream& SearchRoot::info_nps(ostream& o) const {
    std::ostringstream buffer;
    nps(buffer);

    if (!buffer.str().empty()) {
        o << "info" << buffer.str() << '\n';
    }
    return o;
}

ReturnStatus NodeCounter::count(const SearchRoot& root) {
    assertOk();

    if (nodesQuota == 0) {
        return refreshQuota(root);
    }

    assert (nodesQuota > 0);
    --nodesQuota;

    assertOk();
    return ReturnStatus::Continue;
}

ReturnStatus NodeCounter::refreshQuota(const SearchRoot& root) {
    assertOk();
    assert (nodesQuota == 0);
    //nodes -= nodesQuota;

    auto nodesRemaining = nodesLimit - nodes;
    if (nodesRemaining >= QuotaLimit) {
        nodesQuota = QuotaLimit;
    }
    else {
        nodesQuota = static_cast<decltype(nodesQuota)>(nodesRemaining);
        if (nodesQuota == 0) {
            assertOk();
            return ReturnStatus::Abort;
        }
    }

    if (root.isStopped()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return ReturnStatus::Abort;
    }

    assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
    nodes += nodesQuota;
    --nodesQuota; //count current node
    assertOk();

    //inform UCI that search is responsive
    root.readyok();

    return ReturnStatus::Continue;
}
