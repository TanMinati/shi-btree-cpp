#include <iostream>
#include <fstream>
#include <string>

// Simulating your future tree class
class SHIBTree {
public:
    void insert(int key) {
        // Tree insertion logic will go here
    }
    void remove(int key) {
        // Tree deletion logic will go here
    }
};

void runWorkload(const std::string& filename, SHIBTree& tree) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open workload file: " << filename << "\n";
        return;
    }

    char operation;
    int key;
    int op_count = 0;

    while (infile >> operation >> key) {
        if (operation == 'I') {
            tree.insert(key);
        } else if (operation == 'D') {
            tree.remove(key);
        }
        op_count++;
    }

    std::cout << "Successfully processed " << op_count << " operations.\n";
}

int main() {
    SHIBTree myTree;
    
    runWorkload("btree_workload.txt", myTree);

    return 0;
}