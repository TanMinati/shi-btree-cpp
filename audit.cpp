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
    
    std::cout << "Key: " << std::setw(3) << node->key
              << " | Priority: " << std::setw(11) << node->priority
              << " | SubtreeSize: " << std::setw(2) << node->subtreeSize
              << " | RAM Address: " << static_cast<void*>(node) << "\n";

    printMemoryLayout(node->right.get());
}

void runAudit(
    const std::vector<int>& insertTimeline,
    const std::vector<int>& deleteTimeline,
    const std::string& testName
) {
    std::cout << "--- " << testName << " ---\n";

    SHIBTree tree(8);

    for (int key : insertTimeline) {
        tree.insert(key);
    }
    for (int key : deleteTimeline) {
        tree.remove(key);
    }

    BTreeNode* rootNode = tree.getGlobalRoot();

    std::cout << "Final Physical Memory Layout:\n";
    printMemoryLayout(rootNode->partitionRoot);
    std::cout << "\n";
}

int main() {
    // Declare all timelines upfront so heap allocations don't fragment between runs
    std::vector<int> forwardTimeline = {10, 20, 30, 40, 50, 60, 70};
    std::vector<int> reverseTimeline = {70, 60, 50, 40, 30, 20, 10};
    std::vector<int> adversarialInserts = {40, 99, 10, 70, 5, 30, 88, 60, 20, 50};
    std::vector<int> adversarialDeletes = {99, 5, 88};

    // Test A: Keys arrive perfectly in order
    runAudit(forwardTimeline, {}, "Test A: Chronological Insertion");

    // Test B: Keys arrive in completely reverse order
    runAudit(reverseTimeline, {}, "Test B: Reverse Insertion");

    // Test C: Interleaved decoy insertions followed by deletions
    runAudit(adversarialInserts, adversarialDeletes, "Test C: Adversarial Insert + Delete Timeline");

    return 0;
}