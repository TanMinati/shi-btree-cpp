#include <iostream>
#include <vector>
#include <iomanip>
#include "NodePool.hpp"
#include "CartesianNode.hpp"
#include "BTreeNode.hpp"
#include "SHIBTree.hpp"

// In-order traversal for RAM layout
void printMemoryLayout(CartesianNode* node) {
    if (!node) return;
    
    printMemoryLayout(node->left.get());
    
    std::cout << "Key: " << std::setw(3) << node->key << " | Priority: " << std::setw(11) << node->priority << " | RAM Address: " << static_cast<void*>(node) << "\n";

    printMemoryLayout(node->right.get());
}

void runAudit(const std::vector<int>& timeline, const std::string& testName) {
    std::cout << "--- " << testName << " ---\n";

    // Create a fresh tree for this audit case.
    SHIBTree tree(8);

    // Insert keys in the order they arrived.
    for (int key : timeline) {
        tree.insert(key);
    }

    // Get the current root node for inspection.
    BTreeNode* rootNode = tree.getGlobalRoot();

    std::cout << "Final Physical Memory Layout:\n";
    // Walk the Cartesian partition rooted at the current global root.
    printMemoryLayout(rootNode->partitionRoot);
    std::cout << "\n";
}

int main() {
    // Test A: Keys arrive perfectly in order
    std::vector<int> forwardTimeline = {10, 20, 30, 40, 50, 60, 70};
    runAudit(forwardTimeline, "Test A: Chronological Insertion");

    // Test B: Keys arrive in completely reverse order
    std::vector<int> reverseTimeline = {70, 60, 50, 40, 30, 20, 10};
    runAudit(reverseTimeline, "Test B: Reverse Insertion");

    return 0;
}