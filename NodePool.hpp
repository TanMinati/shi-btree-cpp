#ifndef NODE_POOL_HPP
#define NODE_POOL_HPP

#include <iostream>
#include <vector>

template <typename T, size_t NodesPerBlock = 1024>
class NodePool {
private:
    // A union allows us to use the same memory space for the object 
    // OR the free-list pointer when the node is deleted.
    union Chunk {
        T data;
        Chunk* next;
    };

    Chunk* freeList = nullptr;
    std::vector<Chunk*> blocks;

    // Allocates a new massive block of memory from the OS
    void allocateBlock() {
        Chunk* newBlock = static_cast<Chunk*>(::operator new(NodesPerBlock * sizeof(Chunk)));
        blocks.push_back(newBlock);

        // Chain the new chunks together into the free list
        for (size_t i = 0; i < NodesPerBlock - 1; ++i) {
            newBlock[i].next = &newBlock[i + 1];
        }
        newBlock[NodesPerBlock - 1].next = freeList;
        freeList = newBlock;
    }

public:
    NodePool() {}

    ~NodePool() {
        for (Chunk* block : blocks) {
            ::operator delete(block);
        }
    }

    // Grab a node from the free list (O(1) time)
    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!freeList) {
            allocateBlock();
        }
        Chunk* chunk = freeList;
        freeList = freeList->next;
        
        // Construct the object in the pre-allocated memory
        return new (&chunk->data) T(std::forward<Args>(args)...);
    }

    // Return a node to the free list (O(1) time)
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        // Explicitly call the destructor
        ptr->~T();
        
        // Push the chunk back onto the front of the free list
        Chunk* chunk = reinterpret_cast<Chunk*>(ptr);
        chunk->next = freeList;
        freeList = chunk;
    }
};

#endif