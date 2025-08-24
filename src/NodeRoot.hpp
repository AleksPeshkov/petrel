#ifndef NODE_ROOT_HPP
#define NODE_ROOT_HPP

#include "chrono.hpp"
#include "typedefs.hpp"
#include "Node.hpp"
#include "Repetitions.hpp"
#include "Tt.hpp"
#include "UciSearchLimits.hpp"

class Uci;
class NodeRoot;

class HistoryMoves {
    typedef Side::arrayOf<PieceType::arrayOf< Square::arrayOf<Move> >> _t;
    _t v;
public:
    void clear() { std::memset(&v, 0, sizeof(v)); }
    const Move& operator() (Color c, PieceType ty, Square sq) const { return v[c][ty][sq]; }
    void set(Color c, PieceType ty, Square sq, const Move& move) { v[c][ty][sq] = move; }
};

// triangular array
class CACHE_ALIGN PvMoves {
    Ply::arrayOf< Ply::arrayOf<UciMove> > pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void set(Ply ply, UciMove move) {
        pv[ply][0] = move;
        auto target = &pv[ply][1];
        auto source = &pv[ply+1][0];
        while ((*target++ = *source++));
        pv[ply+1][0] = {};
    }

    operator const UciMove* () const { return &pv[0][0]; }

    friend ostream& operator << (ostream& out, const PvMoves& pvMoves) {
        return out << &pvMoves.pv[0][0];
    }
};

class NodeCounter {
    node_count_t nodes = 0; // (0 <= nodes && nodes <= nodesLimit)
    node_count_t nodesLimit; // search limit

    typedef unsigned nodes_quota_t;
    enum : nodes_quota_t { QuotaLimit = 1000 }; // < 0.2ms

    //number of remaining nodes before slow checking for search stop
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

    ReturnStatus count(NodeRoot&);
    ReturnStatus refreshQuota(NodeRoot&);

};

// position extended with repetition history
class NodeRoot : public Node {
public:
    Tt tt;
    Repetitions repetitions;
    PvMoves pvMoves;
    HistoryMoves counterMove;

    NodeCounter nodeCounter;

    UciSearchLimits limits;
    Uci& uci;

protected:
    Color colorToMove_ = White; //root position side to move color

public:
    NodeRoot(Uci& u) : Node{*this}, uci{u} {}

    constexpr Side sideOf(Color color) const { return colorToMove_.is(color) ? My : Op; }
    constexpr Color colorToMove(Ply d = 0) const { return colorToMove_ << d; }

    void newGame();
    void newSearch();
    void newIteration();

    void setHash(size_t);

    ReturnStatus countNode();
};

#endif
