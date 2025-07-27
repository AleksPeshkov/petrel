#include "NodeCounter.hpp"
#include "SearchRoot.hpp"

NodeControl NodeCounter::refreshQuota(const SearchRoot& root) {
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
            return NodeControl::Abort;
        }
    }

    if (root.isStopped()) {
        nodesLimit = nodes;
        nodesQuota = 0;

        assertOk();
        return NodeControl::Abort;
    }

    assert (0 < nodesQuota && nodesQuota <= QuotaLimit);
    nodes += nodesQuota;
    --nodesQuota; //count current node
    assertOk();

    //inform UCI that search is responsive
    root.readyok();

    return NodeControl::Continue;
}
