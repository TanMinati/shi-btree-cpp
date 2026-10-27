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
    
    // Shared arena
    NodePool<CartesianNode, 1024> arena;
    
    int MAX_B;
    int FLANK_SIZE;

    // Finds the child branch for a key
    int findChildIndex(CartesianNode* node, int key) {
        CartesianNode* curr = node;
        int indexEstimate = 0; 
        
        while (curr) {
            if (key < curr->key) {
                curr = curr->left.get();
            } else if (key > curr->key) {
                indexEstimate++; 
                curr = curr->right.get();
            } else {
                return indexEstimate;
            }
        }
        return indexEstimate;
    }

    // Split full child
    void splitChild(BTreeNode* parent, int childIdx) {
        BTreeNode* fullNode = parent->children[childIdx];
        BTreeNode* newSibling = new BTreeNode(fullNode->isLeaf);
        
        std::vector<int> allKeys;
        flattenAndFree(fullNode->partitionRoot, allKeys, arena);
        
        int midIdx = allKeys.size() / 2;
        int promotedKey = allKeys[midIdx];
        
        std::vector<int> leftKeys(allKeys.begin(), allKeys.begin() + midIdx);
        std::vector<int> rightKeys(allKeys.begin() + midIdx + 1, allKeys.end());
        
        fullNode->partitionRoot = ComputeCartesian(leftKeys, FLANK_SIZE, arena);
        fullNode->numKeys = leftKeys.size();
        
        newSibling->partitionRoot = ComputeCartesian(rightKeys, FLANK_SIZE, arena);
        newSibling->numKeys = rightKeys.size();
        
        if (!fullNode->isLeaf) {
            int childMid = fullNode->children.size() / 2;
            newSibling->children.assign(fullNode->children.begin() + childMid, fullNode->children.end());
            fullNode->children.erase(fullNode->children.begin() + childMid, fullNode->children.end());
        }
        
        parent->partitionRoot = RecomputeCartesian(parent->partitionRoot, promotedKey, parent->numKeys, FLANK_SIZE, arena);
        parent->numKeys++;
        
        parent->children.insert(parent->children.begin() + childIdx + 1, newSibling);
    }

    // Insert helper
    void insertNonFull(BTreeNode* node, int key) {
        if (node->isLeaf) {
            node->partitionRoot = RecomputeCartesian(node->partitionRoot, key, node->numKeys, FLANK_SIZE, arena);
            node->numKeys++;
        } else {
            int childIdx = findChildIndex(node->partitionRoot, key);
            
            if (node->children[childIdx]->numKeys >= MAX_B) {
                splitChild(node, childIdx);
                if (key > findPromotedKey(node->partitionRoot, childIdx)) {
                    childIdx++;
                }
            }
            insertNonFull(node->children[childIdx], key);
        }
    }

    // Promotion lookup
    int findPromotedKey(CartesianNode* root, int targetIdx) {
        std::vector<int> tempKeys;
        flattenAndFree(root, tempKeys, arena);
        int val = tempKeys[targetIdx];
        root = ComputeCartesian(tempKeys, FLANK_SIZE, arena); 
        return val;
    }

    // Search helper
    BTreeNode* findNodeContaining(BTreeNode* current, int key) {
        if (!current) return nullptr;
        
        CartesianNode* currCart = current->partitionRoot;
        while (currCart) {
            if (key == currCart->key) return current;
            else if (key < currCart->key) currCart = currCart->left.get();
            else currCart = currCart->right.get();
        }
        
        if (current->isLeaf) return nullptr;
        int childIdx = findChildIndex(current->partitionRoot, key);
        return findNodeContaining(current->children[childIdx], key);
    }

public:
    SHIBTree(int bSize) : MAX_B(bSize), FLANK_SIZE(MAX_B/2), globalRoot(new BTreeNode(true)) {}

    BTreeNode* getGlobalRoot() { return globalRoot; }

    // Insert
    void insert(int key) {
        if (globalRoot->numKeys >= MAX_B) {
            BTreeNode* new_root = new BTreeNode(false);
            new_root->children.push_back(globalRoot);
            splitChild(new_root, 0);
            globalRoot = new_root;
        }
        insertNonFull(globalRoot, key);
    }

    // Remove
    void remove(int key) {
        BTreeNode* target = findNodeContaining(globalRoot, key);
        if (!target) return;
        
        std::vector<int> allKeys;
        flattenAndFree(target->partitionRoot, allKeys, arena);
        
        auto it = std::find(allKeys.begin(), allKeys.end(), key);
        if (it != allKeys.end()) {
            allKeys.erase(it);
            target->numKeys--;
        }
        
        target->partitionRoot = ComputeCartesian(allKeys, FLANK_SIZE, arena);
    }
};

#endif