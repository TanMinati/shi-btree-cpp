#ifndef CARTESIAN_NODE_HPP
#define CARTESIAN_NODE_HPP

#include "TaggedPointer.hpp"

struct CartesianNode {
    int key;
    int priority; // The deterministic pseudo-random hash value
    
    // This stores the left/right children AND the protected flags in exactly 16 bytes.
    TaggedPointer<CartesianNode> left;
    TaggedPointer<CartesianNode> right;

    CartesianNode(int k, int p) : key(k), priority(p), left(nullptr), right(nullptr) {}
};

#endif