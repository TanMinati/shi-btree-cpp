#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <set>
#include <vector>
#include <random>
#include <iomanip>

#include "NodePool.hpp"
#include "CartesianNode.hpp"
#include "CartesianEngine.hpp"
#include "BTreeNode.hpp"
#include "SHIBTree.hpp"

struct Operation {
    char op;
    int key;
};

struct BenchmarkResult {
    int bSize;
    double seconds;
    double opsPerSec;
    double speedupVsSet;
    bool verified;
};

// Loads workload from disk, or generates the deterministic 1M workload in RAM if file is missing
void loadOrGenerateWorkload(const std::string& filename, std::vector<Operation>& workload) {
    workload.clear();
    workload.reserve(1'000'000);

    std::ifstream infile(filename);
    if (infile.is_open()) {
        char op;
        int key;
        while (infile >> op >> key) {
            workload.push_back({op, key});
        }
        infile.close();
        if (!workload.empty()) {
            std::cout << "Loaded " << workload.size() << " operations from " << filename << ".\n\n";
            return;
        }
    }

    std::cout << filename << " not found. Generating 1,000,000 deterministic operations in memory (seed=42)...\n\n";
    const int numOperations = 1'000'000;
    const int keyRange = 5'000'000;
    const double insertProbability = 0.7;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> keyDist(1, keyRange);
    std::uniform_real_distribution<double> actionDist(0.0, 1.0);

    std::vector<int> activeKeys;
    activeKeys.reserve(numOperations);

    for (int i = 0; i < numOperations; ++i) {
        const bool doInsert = actionDist(rng) <= insertProbability;
        if (doInsert || activeKeys.empty()) {
            const int newKey = keyDist(rng);
            activeKeys.push_back(newKey);
            workload.push_back({'I', newKey});
        } else {
            std::uniform_int_distribution<int> indexDist(0, static_cast<int>(activeKeys.size()) - 1);
            const int deleteIndex = indexDist(rng);
            const int keyToDelete = activeKeys[deleteIndex];
            activeKeys[deleteIndex] = activeKeys.back();
            activeKeys.pop_back();
            workload.push_back({'D', keyToDelete});
        }
    }
}

// Helper: In-order traversal of a Cartesian partition (used outside the timer for verification)
void collectCartesianKeys(const CartesianNode* node, std::vector<int>& outKeys) {
    if (!node) return;
    collectCartesianKeys(node->left.get(), outKeys);
    outKeys.push_back(node->key);
    collectCartesianKeys(node->right.get(), outKeys);
}

// Helper: In-order traversal of the entire SHIBTree (used outside the timer for verification)
void collectBTreeKeys(const BTreeNode* node, std::vector<int>& outKeys) {
    if (!node) return;

    std::vector<int> sepKeys;
    sepKeys.reserve(node->numKeys);
    collectCartesianKeys(node->partitionRoot, sepKeys);

    if (node->isLeaf) {
        outKeys.insert(outKeys.end(), sepKeys.begin(), sepKeys.end());
    } else {
        for (size_t i = 0; i < node->children.size(); ++i) {
            collectBTreeKeys(node->children[i], outKeys);
            if (i < sepKeys.size()) {
                outKeys.push_back(sepKeys[i]);
            }
        }
    }
}

// Benchmarks std::set (Red-Black Tree) and returns the final sorted keys for verification
double benchmarkStdSet(const std::vector<Operation>& workload, std::vector<int>& expectedKeys) {
    std::set<int> std_tree;
    const size_t count = workload.size();

    std::cout << "--- Benchmarking std::set (Red-Black Tree) ---\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& item : workload) {
        if (item.op == 'I') {
            std_tree.insert(item.key);
        } else if (item.op == 'D') {
            std_tree.erase(item.key);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    double seconds = diff.count();
    double throughput = count / seconds;

    std::cout << "Operations Processed: " << count << "\n";
    std::cout << "Total Time:           " << std::fixed << std::setprecision(4) << seconds << " seconds\n";
    std::cout << "Throughput:           " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n\n";

    expectedKeys.assign(std_tree.begin(), std_tree.end());
    return seconds;
}

// Benchmarks SHIBTree for a given B size and verifies exact set equality after stopping the timer
BenchmarkResult benchmarkSHIBTree(
    const std::vector<Operation>& workload,
    int b_size,
    double stdSetSeconds,
    const std::vector<int>& expectedKeys
) {
    SHIBTree myTree(b_size);
    const size_t count = workload.size();

    std::cout << "--- Benchmarking SHI B-Tree (B=" << b_size << ") ---\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& item : workload) {
        if (item.op == 'I') {
            myTree.insert(item.key);
        } else if (item.op == 'D') {
            myTree.remove(item.key);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    double seconds = diff.count();
    double throughput = count / seconds;
    double speedup = stdSetSeconds / seconds;

    // Verify correctness outside the timed section
    std::vector<int> actualKeys;
    actualKeys.reserve(expectedKeys.size());
    collectBTreeKeys(myTree.getGlobalRoot(), actualKeys);
    bool verified = (actualKeys == expectedKeys);

    std::cout << "Operations Processed: " << count << "\n";
    std::cout << "Total Time:           " << std::fixed << std::setprecision(4) << seconds << " seconds\n";
    std::cout << "Throughput:           " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";
    std::cout << "Speedup vs std::set:  " << std::fixed << std::setprecision(2) << speedup << "x\n";
    std::cout << "Set Verification:     " << (verified ? "PASSED" : "FAILED") << " (" << actualKeys.size() << " active keys)\n\n";

    return {b_size, seconds, throughput, speedup, verified};
}

int main() {
    const std::string workload_file = "btree_workload.txt";

    std::vector<Operation> workload;
    loadOrGenerateWorkload(workload_file, workload);

    std::cout << "Starting Database Engine Benchmarks...\n\n";

    std::vector<int> expectedKeys;
    double stdSetSeconds = benchmarkStdSet(workload, expectedKeys);
    double stdSetThroughput = workload.size() / stdSetSeconds;

    std::vector<BenchmarkResult> results;
    for (int b = 2; b <= 1024; b *= 2) {
        results.push_back(benchmarkSHIBTree(workload, b, stdSetSeconds, expectedKeys));
    }

    // Print Summary Table
    std::cout << "=========================================================================\n";
    std::cout << "                     BENCHMARK SUMMARY (1,000,000 OPS)                   \n";
    std::cout << "=========================================================================\n";
    std::cout << std::left << std::setw(18) << "Structure"
              << std::right << std::setw(14) << "Time (s)"
              << std::setw(18) << "Throughput (ops/s)"
              << std::setw(12) << "Speedup"
              << std::setw(10) << "Status" << "\n";
    std::cout << "-------------------------------------------------------------------------\n";

    std::cout << std::left << std::setw(18) << "std::set (RB-Tree)"
              << std::right << std::setw(14) << std::fixed << std::setprecision(4) << stdSetSeconds
              << std::setw(18) << std::fixed << std::setprecision(0) << stdSetThroughput
              << std::setw(11) << "1.00x"
              << std::setw(10) << "BASELINE" << "\n";

    for (const auto& r : results) {
        std::string label = "SHIBTree (B=" + std::to_string(r.bSize) + ")";
        std::cout << std::left << std::setw(18) << label
                  << std::right << std::setw(14) << std::fixed << std::setprecision(4) << r.seconds
                  << std::setw(18) << std::fixed << std::setprecision(0) << r.opsPerSec
                  << std::setw(10) << std::fixed << std::setprecision(2) << r.speedupVsSet << "x"
                  << std::setw(10) << (r.verified ? "PASS" : "FAIL") << "\n";
    }
    std::cout << "=========================================================================\n";

    return 0;
}