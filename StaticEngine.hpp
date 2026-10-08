#ifndef CARTESIAN_ENGINE_HPP
#define CARTESIAN_ENGINE_HPP

#include <vector>
#include <cstdint>
#include "CartesianNode.hpp"
#include "NodePool.hpp"

// The Deterministic Hash (SplitMix64 Variant)
inline int getPriority(int key) {
    uint64_t z = (key + 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<int>(z ^ (z >> 31)); 
}

//Right-Spine Stack Algorithm
inline CartesianNode* ComputeCartesian(const std::vector<int>& sortedKeys, int flankSize, NodePool<CartesianNode, 1024>& arena) {
    int n = sortedKeys.size();
    if (n == 0) return nullptr;

    std::vector<CartesianNode*> nodes(n);
    std::vector<bool> isProtected(n, false);

    //Allocate and flag the flanks
    for (int i = 0; i < n; ++i) {
        nodes[i] = arena.allocate(sortedKeys[i], getPriority(sortedKeys[i]));
        if (i < flankSize || i >= n - flankSize) {
            isProtected[i] = true;
        }
    }

    std::vector<int> stack;
    stack.reserve(n);

    //Build the tree
    for (int i = 0; i < n; ++i) {
        int lastPoppedIdx = -1;
        
        while (!stack.empty() && nodes[stack.back()]->priority < nodes[i]->priority) {
            lastPoppedIdx = stack.back();
            stack.pop_back();
        }
        
        if (lastPoppedIdx != -1) {
            TaggedPointer<CartesianNode> ptr(nodes[lastPoppedIdx]);
            ptr.setProtected(isProtected[lastPoppedIdx]);
            nodes[i]->left = ptr;
        }
        
        if (!stack.empty()) {
            TaggedPointer<CartesianNode> ptr(nodes[i]);
            ptr.setProtected(isProtected[i]);
            nodes[stack.back()]->right = ptr;
        }
        
        stack.push_back(i);
    }

    return nodes[stack.front()]; 
}

#endif