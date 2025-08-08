#include "NodeRoot.hpp"
#include "Uci.hpp"

void NodeRoot::setHash(size_t bytes) {
    tt.setSize(bytes);
}

void NodeRoot::newGame() {
    tt.newGame();
    newSearch();
}

void NodeRoot::newSearch() {
    searchStartTime = ::timeNow();
    tt.newSearch();
    counterMove.clear();
    pvMoves.clear();
}

void NodeRoot::newIteration() {
    tt.newIteration();
}

ReturnStatus NodeRoot::countNode() {
    return nodeCounter.count(uci);
}

ReturnStatus NodeCounter::count(const Uci& uci) {
    assertOk();

    if (nodesQuota == 0) {
        return refreshQuota(uci);
    }

    assert (nodesQuota > 0);
    --nodesQuota;

    assertOk();
    return ReturnStatus::Continue;
}

ReturnStatus NodeCounter::refreshQuota(const Uci& uci) {
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
            return ReturnStatus::Stop;
        }
    }

    if (uci.isStopped()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return ReturnStatus::Stop;
    }

    assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
    nodes += nodesQuota;
    --nodesQuota; //count current node
    assertOk();

    //inform UCI that search is responsive
    uci.info_nps_readyok();

    return ReturnStatus::Continue;
}
