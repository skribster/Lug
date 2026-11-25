#pragma once

#include "lug_common.h"

namespace Lug {
    class VMCore; // Forward declare VMCore
}


namespace Lug::Memory {

inline constexpr double GROW_CAPACITY_FACTOR = 2.0;

void* Reallocate(VMCore& vm, void* pointer, std::size_t oldSize, std::size_t newSize);

/**
 * Grows a dynamic array to the new capacity.
 */
template<typename T>
[[nodiscard]] T* GrowArray(VMCore& vm, T* array, std::size_t oldCapacity, std::size_t newCapacity) {
    return (T*)Reallocate(vm, array, sizeof(T) * oldCapacity, sizeof(T) * newCapacity);
}

/**
 * Frees a dynamic array from memory.
 */ 
template<typename T>
void FreeArray(VMCore& vm, T* array, std::size_t currentCapacity) {
    // We route freeing to the Reallocate function, but just set
    // the new size to 0. We have to route all memory functions to
    // Reallocate because the VM needs to keep track of how mnay bytes
    // it allocates so it knows when to garbage collect.
    Reallocate(vm, array, sizeof(T) * currentCapacity, 0);
}


/**
 * Allocates a memory block of given type with
 * a given count.
 */
template<typename T>
[[nodiscard]] T* Allocate(VMCore& vm, std::size_t count = 1) {
    return (T*)Reallocate(vm, nullptr, 0, sizeof(T) * count);
}


/**
 * Frees a given pointer, also calling its destructor.
 */
template<typename T>
void Free(VMCore& vm, T* pointer) {
    pointer->~T();
    Reallocate(vm, pointer, sizeof(T), 0);
}

/**
 * Calculates a new capacity based on a given
 * current capacity. The minimum capacity is
 * 8 elements.
 */
constexpr std::size_t GrowCapacity(std::size_t capacity) {
    return capacity < 8 ? 8 : capacity * GROW_CAPACITY_FACTOR;
}

}