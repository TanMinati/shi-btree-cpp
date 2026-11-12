#ifndef BTREE_NODE_HPP
#define BTREE_NODE_HPP

#include <vector>
#include <cstdint>
#include "CartesianNode.hpp"

struct BTreeNode {
    // Root of the Cartesian partition associated with this node.
    CartesianNode* partitionRoot;

    // Child pointers for internal nodes.
    std::vector<BTreeNode*> children;

    // Number of keys stored in this node.
    int numKeys;

    // Level of this node in the B-tree.
    uint16_t level;

    // Indicates whether this node is a leaf.
    bool isLeaf;

    // Constructs a B-tree node.
    BTreeNode(bool leaf, int maxB = 0, uint16_t nodeLevel = 0)
        : partitionRoot(nullptr), numKeys(0), level(nodeLevel), isLeaf(leaf) {
        if (!leaf && maxB > 0) {
            // Reserve space for the child pointers of an internal node.
            children.reserve(maxB + 2);
        }
    }
};

#endif