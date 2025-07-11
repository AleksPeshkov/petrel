#ifndef NODE_PERFT_HPP
#define NODE_PERFT_HPP

#include "Node.hpp"

class NodePerft : public Node {
protected:
    using Node::Node;
    virtual NodeControl visit(Square from, Square to);
public:
    NodeControl visitChildren() override;
};

#endif
