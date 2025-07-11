#include "NodePerftTT.hpp"
#include "PerftTT.hpp"
#include "SearchControl.hpp"

NodeControl NodePerftTT::visit(Square from, Square to) {
    assert (draft >= 2);
    setZobrist(parent, from, to);

    auto n = control.tt().get(getZobrist(), draft-2);
    if (n != NodeCountNone) {
        perft = n;
    }
    else {
        makeMove(parent, from, to);

        switch (draft) {
            case 2:
                RETURN_IF_ABORT ( static_cast<NodePerft>(*this).visitChildren() );
                break;

            default:
                assert (draft >= 3);
                RETURN_IF_ABORT ( static_cast<NodePerftTT>(*this).visitChildren() );
        }

        control.tt().set(getZobrist(), draft-2, perft);
    }

    updateParentPerft();
    return NodeControl::Continue;
}
