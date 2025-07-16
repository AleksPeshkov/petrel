#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "Node.hpp"

class NodePerftTT;

class NodePerft : public Node {
protected:
    NodePerftTT& parent; //virtual

    virtual NodeControl visit(Square from, Square to);
    NodePerft (const PositionMoves& p, SearchControl& c);

public:
    NodePerft (NodePerftTT& n);
    NodeControl visitChildren() override;
};

#endif
