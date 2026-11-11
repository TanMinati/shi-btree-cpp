#ifndef NODE_POOL_HPP
#define NODE_POOL_HPP

#include <iostream>
#include <vector>
#include <type_traits>

template <typename T, size_t NodesPerBlock = 1024>
class NodePool {
private:
    union Chunk {
        T data;
        Chunk* next;
    };

    Chunk* freeList = nullptr;
    std::vector<Chunk*> blocks;

    void allocateBlock() {
        Chunk* newBlock = static_cast<Chunk*>(::operator new(NodesPerBlock * sizeof(Chunk)));
        blocks.push_back(newBlock);

        for (size_t i = 0; i < NodesPerBlock - 1; ++i) {
            newBlock[i].next = &newBlock[i + 1];
        }
        newBlock[NodesPerBlock - 1].next = freeList;
        freeList = newBlock;
    }

public:
    NodePool() {
        allocateBlock();
    }

    ~NodePool() {
        for (Chunk* block : blocks) {
            ::operator delete(block);
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!freeList) {
            allocateBlock();
        }
        Chunk* chunk = freeList;
        freeList = freeList->next;
        
        return new (&chunk->data) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        
        Chunk* chunk = reinterpret_cast<Chunk*>(ptr);
        if (!freeList || chunk < freeList) {
            chunk->next = freeList;
            freeList = chunk;
        } else {
            Chunk* curr = freeList;
            int steps = 0;
            while (curr->next && curr->next < chunk && ++steps < 64) {
                curr = curr->next;
            }
            chunk->next = curr->next;
            curr->next = chunk;
        }
    }
};

#endif