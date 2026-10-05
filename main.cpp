#include <iostream>
#include "NodePool.hpp"
#include "CartesianNode.hpp"
// This is the test code in main.cpp that demonstrates the usage of the NodePool and TaggedPointer classes with CartesianNode.
int main() {
    // 1. Instantiate the memory pool specifically for CartesianNodes
    NodePool<CartesianNode, 1024> arena;

    // 2. Allocate a raw node from the pool
    CartesianNode* rawNode = arena.allocate(42, 998244353);

    // 3. Wrap it in your tagged pointer
    TaggedPointer<CartesianNode> myNode(rawNode);

    // 4. Test the bitwise masking
    myNode.setProtected(true);

    if (myNode.isProtected()) {
        std::cout << "Node " << myNode->key << " successfully allocated and protected.\n";
    }

    // 5. Clean up (simulating a deletion during dynamic repartitioning)
    arena.deallocate(myNode.get());

    return 0;
}