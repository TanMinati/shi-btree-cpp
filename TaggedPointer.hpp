#ifndef TAGGED_POINTER_HPP
#define TAGGED_POINTER_HPP

#include <cstdint>
#include <cassert>

template <typename T>
class TaggedPointer {
private:
    // uintptr_t is an unsigned integer guaranteed to be large enough to hold a pointer
    uintptr_t tagged_ptr;

    // Mask for the lowest 3 bits (0b111 = 7)
    static constexpr uintptr_t FLAGS_MASK = 0x7;
    static constexpr uintptr_t PTR_MASK = ~FLAGS_MASK;

public:
    // Define the specific flags for your (B, 2)-partitioning
    static constexpr uintptr_t FLAG_PROTECTED = 0x1; // Bit 0
    static constexpr uintptr_t FLAG_FLANK_LEFT = 0x2; // Bit 1 

    TaggedPointer(T* ptr = nullptr) : tagged_ptr(reinterpret_cast<uintptr_t>(ptr)) {
        // Ensure the raw pointer doesn't already have data in the lowest 3 bits
        assert((reinterpret_cast<uintptr_t>(ptr) & FLAGS_MASK) == 0 && "Pointer is not aligned!");
    }

    // --- Pointer Operations ---
    
    T* get() const {
        return reinterpret_cast<T*>(tagged_ptr & PTR_MASK);
    }

    T* operator->() const {
        return get();
    }

    // --- Flag Operations ---

    bool isProtected() const {
        return (tagged_ptr & FLAG_PROTECTED) != 0;
    }

    void setProtected(bool is_protected) {
        if (is_protected) {
            tagged_ptr |= FLAG_PROTECTED; // Set bit to 1
        } else {
            tagged_ptr &= ~FLAG_PROTECTED; // Clear bit to 0
        }
    }
};

#endif