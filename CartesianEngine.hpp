#ifndef CARTESIAN_ENGINE_HPP
#define CARTESIAN_ENGINE_HPP

#include <vector>
#include <cstdint>
#include <algorithm>
#include "CartesianNode.hpp"
#include "NodePool.hpp"

// The Deterministic Hash (SplitMix64 Variant)
inline int getPriority(int key) {
    uint64_t z = (key + 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<int>(z ^ (z >> 31)); 
}

// Flattens a sub-tree into a sorted array and frees the old nodes
inline void flattenAndFree(CartesianNode* node, std::vector<int>& out_keys, NodePool<CartesianNode, 1024>& arena) {
    if (!node) return;
    
    // In-order traversal
    flattenAndFree(node->left.get(), out_keys, arena);
    out_keys.push_back(node->key);
    flattenAndFree(node->right.get(), out_keys, arena);
    
    // Return the physical memory address to the LIFO free-list
    arena.deallocate(node); 
}

inline CartesianNode* ComputeCartesian(const std::vector<int>& sortedKeys, int flankSize, NodePool<CartesianNode, 1024>& arena) {
    int n = sortedKeys.size();
    if (n == 0) return nullptr;

    // Fixed-size stack workspace.
    CartesianNode* nodes[1024];
    bool isProtected[1024];
    int stack[1024];
    int stackTop = -1;

    for (int i = 0; i < n; ++i) {
        nodes[i] = arena.allocate(sortedKeys[i], getPriority(sortedKeys[i]));
    }

    std::sort(nodes, nodes + n);

    for (int i = 0; i < n; ++i) {
        nodes[i]->key = sortedKeys[i];
        nodes[i]->priority = getPriority(sortedKeys[i]);
        isProtected[i] = (i < flankSize || i >= n - flankSize);
    }

    for (int i = 0; i < n; ++i) {
        int lastPoppedIdx = -1;

        while (stackTop >= 0 && nodes[stack[stackTop]]->priority < nodes[i]->priority) {
            lastPoppedIdx = stack[stackTop];
            stackTop--;
        }

        if (lastPoppedIdx != -1) {
            TaggedPointer<CartesianNode> ptr(nodes[lastPoppedIdx]);
            ptr.setProtected(isProtected[lastPoppedIdx]);
            nodes[i]->left = ptr;
        }

        if (stackTop >= 0) {
            TaggedPointer<CartesianNode> ptr(nodes[i]);
            ptr.setProtected(isProtected[i]);
            nodes[stack[stackTop]]->right = ptr;
        }

        stack[++stackTop] = i;
    }

    return nodes[stack[0]];
}

// Dynamically inserts a key and recomputes the affected region
inline CartesianNode* RecomputeCartesian(CartesianNode* globalRoot, int newKey, int currentSize, int flankSize, NodePool<CartesianNode, 1024>& arena) {
    // Small tree: rebuild from scratch.
    if (currentSize < 2 * flankSize) {
        std::vector<int> allKeys;
        flattenAndFree(globalRoot, allKeys, arena);

        auto it = std::lower_bound(allKeys.begin(), allKeys.end(), newKey);
        allKeys.insert(it, newKey);

        return ComputeCartesian(allKeys, flankSize, arena);
    }

    // Find the unprotected boundary.
    CartesianNode* unprotectedSubroot = nullptr;
    CartesianNode* parentSubroot = nullptr;

    CartesianNode* curr = globalRoot;
    CartesianNode* parent = nullptr;

    while (curr) {
        bool isProtected = true;

        if (parent != nullptr) {
            if (curr->key < parent->key) {
                isProtected = parent->left.isProtected();
            } else {
                isProtected = parent->right.isProtected();
            }
        }

        if (!isProtected) {
            unprotectedSubroot = curr;
            parentSubroot = parent;
            break;
        }

        parent = curr;
        if (newKey < curr->key) {
            curr = curr->left.get();
        } else if (newKey > curr->key) {
            curr = curr->right.get();
        } else {
            break;
        }
    }

    // Fallback for protected-only paths.
    if (!unprotectedSubroot) {
        unprotectedSubroot = globalRoot;
        parentSubroot = nullptr;
    }

    // Extract and rebuild the unprotected region.
    std::vector<int> interiorKeys;
    flattenAndFree(unprotectedSubroot, interiorKeys, arena);

    auto it = std::lower_bound(interiorKeys.begin(), interiorKeys.end(), newKey);
    interiorKeys.insert(it, newKey);

    CartesianNode* newSubroot = ComputeCartesian(interiorKeys, 0, arena);

    // Reattach the rebuilt subtree.
    if (parentSubroot) {
        if (newSubroot->key < parentSubroot->key) {
            TaggedPointer<CartesianNode> ptr(newSubroot);
            ptr.setProtected(false);
            parentSubroot->left = ptr;
        } else {
            TaggedPointer<CartesianNode> ptr(newSubroot);
            ptr.setProtected(false);
            parentSubroot->right = ptr;
        }
        return globalRoot;
    }

    return newSubroot;
}

#endif