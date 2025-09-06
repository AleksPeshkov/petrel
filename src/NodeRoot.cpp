#include "NodeRoot.hpp"
#include "Uci.hpp"

void NodeRoot::setHash(size_t bytes) {
    tt.setSize(bytes);
}

void NodeRoot::newGame() {
    tt.newGame();
    counterMove.clear();
    newSearch();
}

void NodeRoot::newSearch() {
    tt.newSearch();
    pvMoves.clear();
    pvScore = NoScore;
}

void NodeRoot::newIteration() {
    tt.newIteration();
}

ReturnStatus NodeRoot::countNode() {
    return nodeCounter.count(root.limits);
}

ReturnStatus NodeCounter::count(UciSearchLimits& limits) {
    assertOk();

    if (nodesQuota == 0) {
        return refreshQuota(limits);
    }

    assert (nodesQuota > 0);
    --nodesQuota;

    assertOk();
    return ReturnStatus::Continue;
}

ReturnStatus NodeCounter::refreshQuota(UciSearchLimits& limits) {
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

    if (limits.isStopped()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return ReturnStatus::Stop;
    }

    if (limits.hardDeadlineReached()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return ReturnStatus::Stop;
    }

    assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
    nodes += nodesQuota;
    --nodesQuota; //count current node
    assertOk();

    return ReturnStatus::Continue;
}
