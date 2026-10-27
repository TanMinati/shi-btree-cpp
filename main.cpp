#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <set>
#include <vector>

#include "NodePool.hpp"
#include "CartesianNode.hpp"
#include "CartesianEngine.hpp"
#include "BTreeNode.hpp"
#include "SHIBTree.hpp"

// Benchmarks the SHI B-Tree
void benchmarkSHIBTree(const std::string& filename, int b_size) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open " << filename << "\n";
        return;
    }

    SHIBTree myTree(b_size);
    char op;
    int key;
    int count = 0;

    std::cout << "--- Benchmarking SHI B-Tree (B=" << b_size << ") ---\n";
    
    auto start = std::chrono::high_resolution_clock::now();

    while (infile >> op >> key) {
        if (op == 'I') {
            myTree.insert(key);
        } else if (op == 'D') {
            myTree.remove(key); // Assuming remove is fully implemented
        }
        count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Operations Processed: " << count << "\n";
    std::cout << "Total Time: " << diff.count() << " seconds\n";
    std::cout << "Throughput: " << (count / diff.count()) << " ops/sec\n\n";
}

// Benchmarks standard C++ Red-Black Tree for comparison
void benchmarkStdSet(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) return;

    std::set<int> std_tree;
    char op;
    int key;
    int count = 0;

    std::cout << "--- Benchmarking std::set (Red-Black Tree) ---\n";
    
    auto start = std::chrono::high_resolution_clock::now();

    while (infile >> op >> key) {
        if (op == 'I') {
            std_tree.insert(key);
        } else if (op == 'D') {
            std_tree.erase(key);
        }
        count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Operations Processed: " << count << "\n";
    std::cout << "Total Time: " << diff.count() << " seconds\n";
    std::cout << "Throughput: " << (count / diff.count()) << " ops/sec\n\n";
}

int main() {
    const std::string workload_file = "btree_workload.txt";

    std::cout << "Starting Database Engine Benchmarks...\n\n";

    benchmarkStdSet(workload_file);

    for(int i=2;i<=1024;i*=2) {
        benchmarkSHIBTree(workload_file, i);
    }

    return 0;
}