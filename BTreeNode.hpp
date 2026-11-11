#ifndef BTREE_NODE_HPP
#define BTREE_NODE_HPP

#include <vector>
#include <cstdint>
#include "CartesianNode.hpp"

struct BTreeNode {
    // 8-byte aligned pointer first to eliminate internal struct padding
    CartesianNode* partitionRoot;

    // Child pointers for internal nodes (24 bytes)
    std::vector<BTreeNode*> children;

    int numKeys;
    uint16_t level; // Height level i for independent hash function h_i
    bool isLeaf;

    BTreeNode(bool leaf, int maxB = 0, uint16_t nodeLevel = 0)
        : partitionRoot(nullptr), numKeys(0), level(nodeLevel), isLeaf(leaf) {
        if (!leaf && maxB > 0) {
            children.reserve(maxB + 2);
        }
    }
};

#endif