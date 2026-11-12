#ifndef CARTESIAN_ENGINE_HPP
#define CARTESIAN_ENGINE_HPP

#include <vector>
#include <cstdint>
#include <algorithm>
#include "CartesianNode.hpp"
#include "NodePool.hpp"

// Level-Independent Deterministic Hash (SplitMix64 Variant)
// Implements independent hash functions h_0, h_1, ... per B-Tree level
inline int getPriority(int key, uint64_t level = 0) {
    uint64_t z = (static_cast<uint64_t>(key) ^ (level * 0x9e3779b97f4a7c15ULL)) + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<int>(z ^ (z >> 31)); 
}

// Helper: collects sorted keys and nodes in ascending address/key order
inline void flattenCollect(CartesianNode* node, std::vector<int>& outKeys, std::vector<CartesianNode*>& outNodes) {
    if (!node) return;
    flattenCollect(node->left.get(), outKeys, outNodes);
    outKeys.push_back(node->key);
    outNodes.push_back(node);
    flattenCollect(node->right.get(), outKeys, outNodes);
}

// Flattens a sub-tree into a sorted array and frees nodes in descending address order
inline void flattenAndFree(CartesianNode* node, std::vector<int>& outKeys, NodePool<CartesianNode, 1024>& arena) {
    if (!node) return;
    static thread_local std::vector<CartesianNode*> freedNodes;
    freedNodes.clear();
    flattenCollect(node, outKeys, freedNodes);
    for (int i = static_cast<int>(freedNodes.size()) - 1; i >= 0; --i) {
        arena.deallocate(freedNodes[i]);
    }
}

// Thread-local workspaces eliminate heap allocations across all hot paths
inline std::vector<CartesianNode*>& getWorkspaceNodes() { static thread_local std::vector<CartesianNode*> v; return v; }
inline std::vector<uint8_t>& getWorkspaceProtected() { static thread_local std::vector<uint8_t> v; return v; }
inline std::vector<int>& getWorkspaceStack() { static thread_local std::vector<int> v; return v; }
inline std::vector<CartesianNode*>& getWorkspaceAncestors() { static thread_local std::vector<CartesianNode*> v; return v; }
inline std::vector<int>& getWorkspaceKeys() { static thread_local std::vector<int> v; return v; }

// Cartesian tree constructor
inline CartesianNode* ComputeCartesian(const int* sortedKeys,int n,int globalOffset,int totalN,int flankSize,NodePool<CartesianNode, 1024>& arena,uint64_t level = 0) {
    if (n <= 0) return nullptr;

    auto& nodes = getWorkspaceNodes();
    auto& isProtected = getWorkspaceProtected();
    auto& stack = getWorkspaceStack();
    
    nodes.resize(n);
    isProtected.resize(n);
    stack.resize(n);
    
    int stackTop = -1;

    //Allocate raw nodes from the arena
    for (int i = 0; i < n; ++i) {
        nodes[i] = arena.allocate(0, 0);
    }

    // Sort physical RAM addresses only if not already ordered
    if (!std::is_sorted(nodes.begin(), nodes.end())) {
        std::sort(nodes.begin(), nodes.end());
    }

    // Single-pass initialization of key, level-seeded priority, and global flank protection
    for (int i = 0; i < n; ++i) {
        int k = sortedKeys[i];
        nodes[i]->key = k;
        nodes[i]->priority = getPriority(k, level);
        
        int globalIdx = globalOffset + i;
        isProtected[i] = (globalIdx < flankSize || globalIdx >= totalN - flankSize) ? 1 : 0;
    }

    // O(n) monotonic stack Cartesian tree construction
    for (int i = 0; i < n; ++i) {
        int lastPoppedIdx = -1;

        while (stackTop >= 0 && nodes[stack[stackTop]]->priority < nodes[i]->priority) {
            lastPoppedIdx = stack[stackTop];
            stackTop--;
            updateSubtreeSize(nodes[lastPoppedIdx]);
        }

        if (lastPoppedIdx != -1) {
            TaggedPointer<CartesianNode> ptr(nodes[lastPoppedIdx]);
            ptr.setProtected(isProtected[lastPoppedIdx] != 0);
            nodes[i]->left = ptr;
        }

        if (stackTop >= 0) {
            TaggedPointer<CartesianNode> ptr(nodes[i]);
            ptr.setProtected(isProtected[i] != 0);
            nodes[stack[stackTop]]->right = ptr;
        }

        stack[++stackTop] = i;
    }

    for (int i = stackTop; i >= 0; --i) {
        updateSubtreeSize(nodes[stack[i]]);
    }

    return nodes[stack[0]];
}

// Vector wrapper overload
inline CartesianNode* ComputeCartesian(const std::vector<int>& sortedKeys,int globalOffset,int totalN,int flankSize,NodePool<CartesianNode, 1024>& arena,uint64_t level = 0) {
    return ComputeCartesian(sortedKeys.data(), static_cast<int>(sortedKeys.size()), globalOffset, totalN, flankSize, arena, level);
}

// Dynamically inserts a key in O(log B) expected time with single-pass duplicate detection
inline CartesianNode* RecomputeCartesian(CartesianNode* globalRoot,int newKey,int currentSize,int flankSize,NodePool<CartesianNode, 1024>& arena,uint64_t level = 0,bool* outInserted = nullptr) {
    if (outInserted) *outInserted = true;

    auto& keyScratch = getWorkspaceKeys();
    
    // Small tree 
    if (!globalRoot || currentSize < 2 * flankSize) {
        // Fast check for duplicate before flattening small tree
        CartesianNode* check = globalRoot;
        while (check) {
            if (newKey == check->key) {
                if (outInserted) *outInserted = false;
                return globalRoot;
            }
            check = (newKey < check->key) ? check->left.get() : check->right.get();
        }

        int totalN = currentSize + 1;
        keyScratch.clear();
        flattenAndFree(globalRoot, keyScratch, arena);
        auto it = std::lower_bound(keyScratch.begin(), keyScratch.end(), newKey);
        keyScratch.insert(it, newKey);
        return ComputeCartesian(keyScratch.data(), static_cast<int>(keyScratch.size()), 0, totalN, flankSize, arena, level);
    }

    CartesianNode* unprotectedSubroot = nullptr;
    CartesianNode* parentSubroot = nullptr;
    CartesianNode* curr = globalRoot;
    CartesianNode* parent = nullptr;
    
    auto& ancestors = getWorkspaceAncestors();
    ancestors.clear();
    
    int leftOffset = 0;
    int unprotectedOffset = 0;
    int newKeyPriority = getPriority(newKey, level);

    // Walk down until newKey's priority beats curr->priority or a duplicate is found
    while (curr) {
        if (newKey == curr->key) {
            if (outInserted) *outInserted = false;
            return globalRoot;
        }

        if (newKeyPriority > curr->priority) {
            // Confirm newKey is not already inside curr's subtree before flattening
            CartesianNode* dupCheck = (newKey < curr->key) ? curr->left.get() : curr->right.get();
            while (dupCheck) {
                if (newKey == dupCheck->key) {
                    if (outInserted) *outInserted = false;
                    return globalRoot;
                }
                dupCheck = (newKey < dupCheck->key) ? dupCheck->left.get() : dupCheck->right.get();
            }

            unprotectedSubroot = curr;
            parentSubroot = parent;
            unprotectedOffset = leftOffset;
            break;
        }

        ancestors.push_back(curr);
        parent = curr;
        
        if (newKey < curr->key) {
            curr = curr->left.get();
        } else {
            leftOffset += 1 + getSubtreeSize(curr->left.get());
            curr = curr->right.get();
        }
    }

    int totalN = currentSize + 1;

    // Fast path: newKey has lower priority than all ancestors along the search path -> attach 1 leaf node
    if (!unprotectedSubroot) {
        CartesianNode* newLeaf = arena.allocate(newKey, newKeyPriority);
        bool leafProtected = (leftOffset < flankSize || leftOffset >= totalN - flankSize);
        TaggedPointer<CartesianNode> ptr(newLeaf);
        ptr.setProtected(leafProtected);

        if (newKey < parent->key) {
            parent->left = ptr;
        } else {
            parent->right = ptr;
        }

        for (int i = static_cast<int>(ancestors.size()) - 1; i >= 0; --i) {
            updateSubtreeSize(ancestors[i]);
        }
        return globalRoot;
    }

    // Rebuild only the affected subtree rooted at unprotectedSubroot
    keyScratch.clear();
    flattenAndFree(unprotectedSubroot, keyScratch, arena);

    auto it = std::lower_bound(keyScratch.begin(), keyScratch.end(), newKey);
    keyScratch.insert(it, newKey);

    CartesianNode* newSubroot = ComputeCartesian(keyScratch.data(),static_cast<int>(keyScratch.size()),unprotectedOffset,totalN,flankSize,arena,level);

    if (parentSubroot) {
        int newSubrootGlobalRank = unprotectedOffset + getSubtreeSize(newSubroot->left.get());
        bool subrootProtected = (newSubrootGlobalRank < flankSize || newSubrootGlobalRank >= totalN - flankSize);

        TaggedPointer<CartesianNode> ptr(newSubroot);
        ptr.setProtected(subrootProtected);

        if (newSubroot->key < parentSubroot->key) {
            parentSubroot->left = ptr;
        } else {
            parentSubroot->right = ptr;
        }

        for (int i = static_cast<int>(ancestors.size()) - 1; i >= 0; --i) {
            updateSubtreeSize(ancestors[i]);
        }

        return globalRoot;
    }

    return newSubroot;
}

// Dynamically removes a key in O(log B) expected time by rebuilding only the deleted node's subtree
inline CartesianNode* RemoveCartesian(CartesianNode* globalRoot,int delKey,int currentSize,int flankSize,NodePool<CartesianNode, 1024>& arena,uint64_t level = 0,bool* outRemoved = nullptr) {
    if (outRemoved) *outRemoved = false;
    if (!globalRoot) return nullptr;

    auto& keyScratch = getWorkspaceKeys();

    if (currentSize <= 2 * flankSize) {
        keyScratch.clear();
        flattenAndFree(globalRoot, keyScratch, arena);
        auto it = std::lower_bound(keyScratch.begin(), keyScratch.end(), delKey);
        if (it != keyScratch.end() && *it == delKey) {
            keyScratch.erase(it);
            if (outRemoved) *outRemoved = true;
        }
        int rem = static_cast<int>(keyScratch.size());
        return ComputeCartesian(keyScratch.data(), rem, 0, rem, flankSize, arena, level);
    }

    auto& ancestors = getWorkspaceAncestors();
    ancestors.clear();

    CartesianNode* curr = globalRoot;
    CartesianNode* parent = nullptr;
    int leftOffset = 0;

    while (curr && curr->key != delKey) {
        ancestors.push_back(curr);
        parent = curr;
        if (delKey < curr->key) {
            curr = curr->left.get();
        } else {
            leftOffset += 1 + getSubtreeSize(curr->left.get());
            curr = curr->right.get();
        }
    }

    if (!curr) return globalRoot; // Key not found
    if (outRemoved) *outRemoved = true;

    int totalN = currentSize - 1;
    CartesianNode* replacement = nullptr;

    // Fast path 1: leaf or single-child node requires zero rebuilds
    if (!curr->left.get() || !curr->right.get()) {
        replacement = curr->left.get() ? curr->left.get() : curr->right.get();
        arena.deallocate(curr);
    } else {
        // Rebuild only curr's left and right subtrees
        keyScratch.clear();
        flattenAndFree(curr->left.get(), keyScratch, arena);
        flattenAndFree(curr->right.get(), keyScratch, arena);
        arena.deallocate(curr);

        replacement = ComputeCartesian(keyScratch.data(),static_cast<int>(keyScratch.size()),leftOffset,totalN,flankSize,arena,level);
    }

    if (parent) {
        TaggedPointer<CartesianNode> ptr(replacement);
        if (replacement) {
            int repGlobalRank = leftOffset + getSubtreeSize(replacement->left.get());
            bool repProtected = (repGlobalRank < flankSize || repGlobalRank >= totalN - flankSize);
            ptr.setProtected(repProtected);
        }
        if (delKey < parent->key) {
            parent->left = ptr;
        } else {
            parent->right = ptr;
        }

        for (int i = static_cast<int>(ancestors.size()) - 1; i >= 0; --i) {
            updateSubtreeSize(ancestors[i]);
        }
        return globalRoot;
    }

    return replacement;
}

#endif