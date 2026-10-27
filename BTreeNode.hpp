#ifndef BTREE_NODE_HPP
#define BTREE_NODE_HPP

#include <vector>
#include "CartesianNode.hpp"

struct BTreeNode {
    bool isLeaf;
    int numKeys;

    // Cartesian tree root for this node.
    CartesianNode* partitionRoot;

    // Child pointers for internal nodes.
    std::vector<BTreeNode*> children;

    BTreeNode(bool leaf) : isLeaf(leaf), numKeys(0), partitionRoot(nullptr) {}
};

#endif