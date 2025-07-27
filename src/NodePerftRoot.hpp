#ifndef NODE_PERFT_ROOT_HPP
#define NODE_PERFT_ROOT_HPP

#include "NodePerftTT.hpp"
#include "UciGoLimit.hpp"
#include "SearchControl.hpp"

class NodePerftRoot : public NodePerftTT {
    bool isDivide;
public:
    NodePerftRoot(const PositionMoves& p, SearchControl& c, Ply d, bool i)
        : NodePerftTT(p, c, d), isDivide{i} {}

    virtual NodeControl visitRoot();
    virtual NodeControl visitChildren() override;
};

#endif
