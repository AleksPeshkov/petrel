#ifndef NODE_AB_ROOT_HPP
#define NODE_AB_ROOT_HPP

#include "NodeAb.hpp"

class UciGoLimit;

class NodeAbRoot : public NodeAb {
    Ply depthLimit;

public:
    NodeAbRoot (const UciGoLimit&, SearchRoot&);
    NodeControl visitChildren() override;
};

#endif
