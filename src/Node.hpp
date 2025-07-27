#ifndef NODE_HPP
#define NODE_HPP

#include "PositionMoves.hpp"

enum class NodeControl {
    Continue,
    Abort,
    BetaCutoff,
};

#define RETURN_IF_ABORT(visit) { if (visit == NodeControl::Abort) { return NodeControl::Abort; } } ((void)0)
#define BREAK_IF_ABORT(visit) { if (visit == NodeControl::Abort) { break; } } ((void)0)
#define CUTOFF(visit) { NodeControl result = visit; \
    if (result == NodeControl::Abort) { return NodeControl::Abort; } \
    if (result == NodeControl::BetaCutoff) { return NodeControl::BetaCutoff; }} ((void)0)

class SearchRoot;

class Node : public PositionMoves {
protected:
    SearchRoot& root;

    Node (const PositionMoves& p, SearchRoot& r) : PositionMoves{p}, root{r} {}
    Node (SearchRoot& r) : PositionMoves{}, root{r} {}

public:
    virtual ~Node() = default;
    virtual NodeControl visitChildren() = 0;
};

#endif
