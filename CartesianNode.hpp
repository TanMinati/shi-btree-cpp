#ifndef CARTESIAN_NODE_HPP
#define CARTESIAN_NODE_HPP

#include "TaggedPointer.hpp"

struct CartesianNode {
    int key;
    int priority;    // Deterministic pseudo-random hash value
    int subtreeSize; // Total nodes in the subtree rooted at this node
    
    // Stores the left/right children AND the protected flags in 16 bytes
    TaggedPointer<CartesianNode> left;
    TaggedPointer<CartesianNode> right;

    CartesianNode(int k, int p) 
        : key(k), priority(p), subtreeSize(1), left(nullptr), right(nullptr) {}
};

inline int getSubtreeSize(const CartesianNode* node) {
    return node ? node->subtreeSize : 0;
}

inline void updateSubtreeSize(CartesianNode* node) {
    if (node) {
        node->subtreeSize = 1 + getSubtreeSize(node->left.get()) + getSubtreeSize(node->right.get());
    }
}

#endif