#ifndef NODE_COUNTER_HPP
#define NODE_COUNTER_HPP

#include "typedefs.hpp"
#include "Node.hpp"

class SearchRoot;

class NodeCounter {
    node_count_t nodes = 0; // (0 <= nodes && nodes <= nodesLimit)
    node_count_t nodesLimit; // search limit

    typedef unsigned nodes_quota_t;
    enum : nodes_quota_t { QuotaLimit = 1000 }; // ~0.1 msec

    //number of remaining nodes before slow checking for search abort
    nodes_quota_t nodesQuota = 0; // (0 <= nodesQuota && nodesQuota <= QuotaLimit)

    constexpr void assertOk() const {
        assert (nodesQuota <= nodes && nodes <= nodesLimit);
        assert (/* 0 <= nodesQuota && */ nodesQuota < QuotaLimit);
    }

public:
    constexpr NodeCounter(node_count_t n = NodeCountMax) : nodesLimit{n} {}

    /// exact number of visited nodes
    constexpr operator node_count_t () const {
        assertOk();
        return nodes - nodesQuota;
    }

    constexpr bool isAborted() const {
        assertOk();
        assert (nodes - nodesQuota < nodesLimit || nodesQuota == 0);
        return nodes == nodesLimit;
    }

    NodeControl count(const SearchRoot& root) {
        assertOk();

        if (nodesQuota == 0) {
            return refreshQuota(root);
        }

        assert (nodesQuota > 0);
        --nodesQuota;

        assertOk();
        return NodeControl::Continue;
    }

    NodeControl refreshQuota(const SearchRoot&);

};

#endif
