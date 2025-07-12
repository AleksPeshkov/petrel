#include "NodePerftLeaf.hpp"
#include "NodePerft.hpp"
#include "SearchControl.hpp"

NodeControl NodePerftLeaf::visit(Square from, Square to) {
    makeMove(parent, from, to);

    //updateParentPerft:
    auto& p = static_cast<NodePerft&>(parent);
    p.perft += movesCount();

    return NodeControl::Continue;
}
