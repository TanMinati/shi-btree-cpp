#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>

int main() {
    const std::string filename = "btree_workload.txt";
    const int numOperations = 1'000'000;
    const int keyRange = 5'000'000;
    const double insertProbability = 0.7;

    // Create the workload file.
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing.\n";
        return 1;
    }

    // Deterministic workload for reproducible testing.
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> keyDist(1, keyRange);
    std::uniform_real_distribution<double> actionDist(0.0, 1.0);

    // Tracks keys currently present in the tree.
    std::vector<int> activeKeys;
    activeKeys.reserve(numOperations);

    for (int i = 0; i < numOperations; ++i) {
        const bool doInsert = actionDist(rng) <= insertProbability;

        if (doInsert || activeKeys.empty()) {
            const int newKey = keyDist(rng);
            activeKeys.push_back(newKey);
            outfile << "I " << newKey << '\n';
        } else {
            // Remove a random existing key.
            std::uniform_int_distribution<int> indexDist(0, static_cast<int>(activeKeys.size()) - 1);

            const int deleteIndex = indexDist(rng);
            const int keyToDelete = activeKeys[deleteIndex];

            activeKeys[deleteIndex] = activeKeys.back();
            activeKeys.pop_back();

            outfile << "D " << keyToDelete << '\n';
        }
    }

    outfile.close();
    std::cout << "Successfully wrote workload to " << filename << ".\n";

    return 0;
}