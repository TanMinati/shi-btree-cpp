#ifndef SHI_BTREE_HPP
#define SHI_BTREE_HPP

#include <vector>
#include <algorithm>
#include "BTreeNode.hpp"
#include "CartesianEngine.hpp"
#include "NodePool.hpp"

class SHIBTree {
private:
    BTreeNode* globalRoot;
    NodePool<CartesianNode, 1024> arena;
    NodePool<BTreeNode, 256> btreeArena;
    
    int MAX_B;
    int FLANK_SIZE;

    int findChildIndex(CartesianNode* node, int key, bool& isDuplicate) {
        CartesianNode* curr = node;
        int indexEstimate = 0; 
        isDuplicate = false;
        
        while (curr) {
            if (key < curr->key) {
                curr = curr->left.get();
            } else if (key > curr->key) {
                indexEstimate += 1 + getSubtreeSize(curr->left.get());
                curr = curr->right.get();
            } else {
                isDuplicate = true;
                return indexEstimate + getSubtreeSize(curr->left.get());
            }
        }
        return indexEstimate;
    }

    int splitChild(BTreeNode* parent, int childIdx) {
        BTreeNode* fullNode = parent->children[childIdx];
        BTreeNode* newSibling = btreeArena.allocate(fullNode->isLeaf, MAX_B, fullNode->level);
        
        static thread_local std::vector<int> splitKeys;
        splitKeys.clear();
        flattenAndFree(fullNode->partitionRoot, splitKeys, arena);
        
        int n = static_cast<int>(splitKeys.size());
        int midIdx = n / 2;
        int promotedKey = splitKeys[midIdx];
        int rightCount = n - (midIdx + 1);
        
        fullNode->partitionRoot = ComputeCartesian(
            splitKeys.data(), midIdx, 0, midIdx, FLANK_SIZE, arena, fullNode->level
        );
        fullNode->numKeys = midIdx;
        
        newSibling->partitionRoot = ComputeCartesian(
            splitKeys.data() + midIdx + 1, rightCount, 0, rightCount, FLANK_SIZE, arena, newSibling->level
        );
        newSibling->numKeys = rightCount;
        
        if (!fullNode->isLeaf) {
            int childMid = midIdx + 1;
            newSibling->children.assign(fullNode->children.begin() + childMid, fullNode->children.end());
            fullNode->children.erase(fullNode->children.begin() + childMid, fullNode->children.end());
        }
        
        parent->partitionRoot = RecomputeCartesian(
            parent->partitionRoot, promotedKey, parent->numKeys, FLANK_SIZE, arena, parent->level
        );
        parent->numKeys++;
        
        parent->children.insert(parent->children.begin() + childIdx + 1, newSibling);
        return promotedKey;
    }

    void insertNonFull(BTreeNode* node, int key) {
        if (node->isLeaf) {
            bool inserted = false;
            node->partitionRoot = RecomputeCartesian(
                node->partitionRoot, key, node->numKeys, FLANK_SIZE, arena, node->level, &inserted
            );
            if (inserted) {
                node->numKeys++;
            }
            return;
        }

        bool isDuplicate = false;
        int childIdx = findChildIndex(node->partitionRoot, key, isDuplicate);
        if (isDuplicate) return;

        if (node->children[childIdx]->numKeys >= MAX_B) {
            int promotedKey = splitChild(node, childIdx);
            if (key == promotedKey) return;
            if (key > promotedKey) {
                childIdx++;
            }
        }
        insertNonFull(node->children[childIdx], key);
    }

    BTreeNode* findNodeContaining(BTreeNode* current, int key) {
        if (!current) return nullptr;
        bool isDuplicate = false;
        int childIdx = findChildIndex(current->partitionRoot, key, isDuplicate);
        
        if (isDuplicate) return current;
        if (current->isLeaf) return nullptr;
        
        return findNodeContaining(current->children[childIdx], key);
    }

    // Finds the maximum key in a Cartesian tree in O(log B) time
    int findMaxKey(CartesianNode* node) {
        while (node->right.get()) {
            node = node->right.get();
        }
        return node->key;
    }

    bool extractMaxFromSubtree(BTreeNode* node, int& outKey) {
        if (!node) return false;

        if (node->isLeaf) {
            if (node->numKeys == 0 || !node->partitionRoot) return false;
            outKey = findMaxKey(node->partitionRoot);
            node->partitionRoot = RemoveCartesian(
                node->partitionRoot, outKey, node->numKeys, FLANK_SIZE, arena, node->level
            );
            node->numKeys--;
            return true;
        }

        if (extractMaxFromSubtree(node->children.back(), outKey)) {
            return true;
        }

        if (node->numKeys == 0 || !node->partitionRoot) return false;

        btreeArena.deallocate(node->children.back());
        node->children.pop_back();

        outKey = findMaxKey(node->partitionRoot);
        node->partitionRoot = RemoveCartesian(
            node->partitionRoot, outKey, node->numKeys, FLANK_SIZE, arena, node->level
        );
        node->numKeys--;
        return true;
    }

    // Collapses globalRoot back into a single leaf when total keys drop below MAX_B
    void tryCollapseRoot() {
        if (!globalRoot || globalRoot->isLeaf) return;

        int totalKeys = globalRoot->numKeys;
        for (BTreeNode* child : globalRoot->children) {
            if (!child->isLeaf) return;
            totalKeys += child->numKeys;
            if (totalKeys >= MAX_B) return;
        }

        static thread_local std::vector<int> rootSeparators;
        static thread_local std::vector<int> mergedKeys;
        rootSeparators.clear();
        mergedKeys.clear();
        mergedKeys.reserve(totalKeys);

        flattenAndFree(globalRoot->partitionRoot, rootSeparators, arena);

        for (size_t i = 0; i < globalRoot->children.size(); ++i) {
            BTreeNode* child = globalRoot->children[i];
            flattenAndFree(child->partitionRoot, mergedKeys, arena);
            btreeArena.deallocate(child);
            if (i < rootSeparators.size()) {
                mergedKeys.push_back(rootSeparators[i]);
            }
        }

        globalRoot->children.clear();
        globalRoot->isLeaf = true;
        globalRoot->level = 0;
        globalRoot->numKeys = static_cast<int>(mergedKeys.size());
        globalRoot->partitionRoot = ComputeCartesian(
            mergedKeys.data(), globalRoot->numKeys, 0, globalRoot->numKeys, FLANK_SIZE, arena, 0
        );
    }

public:
    SHIBTree(int bSize) : MAX_B(bSize), FLANK_SIZE(std::min(4, MAX_B / 2)) {
        globalRoot = btreeArena.allocate(true, MAX_B, 0);
    }

    BTreeNode* getGlobalRoot() { return globalRoot; }

    void insert(int key) {
        if (globalRoot->numKeys >= MAX_B) {
            BTreeNode* new_root = btreeArena.allocate(false, MAX_B, globalRoot->level + 1);
            new_root->children.push_back(globalRoot);
            int promotedKey = splitChild(new_root, 0);
            globalRoot = new_root;
            if (key == promotedKey) return;
        }
        insertNonFull(globalRoot, key);
    }

    void remove(int key) {
        BTreeNode* target = findNodeContaining(globalRoot, key);
        if (!target) return;

        if (target->isLeaf) {
            bool removed = false;
            target->partitionRoot = RemoveCartesian(
                target->partitionRoot, key, target->numKeys, FLANK_SIZE, arena, target->level, &removed
            );
            if (removed) {
                target->numKeys--;
            }
            tryCollapseRoot();
            return;
        }
        
        static thread_local std::vector<int> targetKeys;
        targetKeys.clear();
        flattenAndFree(target->partitionRoot, targetKeys, arena);
        
        auto it = std::lower_bound(targetKeys.begin(), targetKeys.end(), key);
        if (it == targetKeys.end() || *it != key) {
            int n = static_cast<int>(targetKeys.size());
            target->partitionRoot = ComputeCartesian(targetKeys.data(), n, 0, n, FLANK_SIZE, arena, target->level);
            return;
        }

        int idx = static_cast<int>(it - targetKeys.begin());
        int replacementKey = 0;
        if (extractMaxFromSubtree(target->children[idx], replacementKey)) {
            targetKeys[idx] = replacementKey;
        } else {
            btreeArena.deallocate(target->children[idx]);
            target->children.erase(target->children.begin() + idx);
            targetKeys.erase(it);
            target->numKeys--;
        }
        
        int n = static_cast<int>(targetKeys.size());
        target->partitionRoot = ComputeCartesian(targetKeys.data(), n, 0, n, FLANK_SIZE, arena, target->level);

        tryCollapseRoot();
    }
};

#endif
