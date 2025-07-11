#ifndef NODE_AB_ROOT_HPP
#define NODE_AB_ROOT_HPP

#include "NodeAb.hpp"

class SearchLimit;

class NodeAbRoot : public NodeAb {
    ply_t depthLimit;

public:
    NodeAbRoot (const SearchLimit&, SearchControl&);
    NodeControl visitChildren() override;
};

#endif
