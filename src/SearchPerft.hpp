#ifndef SEARCH_PERFT_HPP
#define SEARCH_PERFT_HPP

#include "SearchRoot.hpp"

class SearchRoot;

class PerftNode : public PositionMoves {
protected:
    SearchRoot& root; /* thread local */
    PerftNode* const parent;
    Ply draft;
    node_count_t perft = 0;

    PerftNode (PerftNode* n) : root{n->root}, parent{n}, draft{n->draft-1} {}

    ReturnStatus visit(Square from, Square to);
    ReturnStatus searchMoves();
    ReturnStatus searchDivide();

public:
    PerftNode (const PositionMoves& p, SearchRoot& r, Ply d) : PositionMoves{p}, root{r}, parent{nullptr}, draft(d) {}
    ReturnStatus visitRoot(bool isDivide);
};

#endif
