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

class SearchControl;

class Node : public PositionMoves {
protected:
    SearchControl& control;

    Node (const PositionMoves& p, SearchControl& c) : PositionMoves{p}, control{c} {}
    Node (SearchControl& c) : PositionMoves{}, control{c} {}

public:
    virtual ~Node() = default;
    virtual NodeControl visitChildren() = 0;
};

#endif
