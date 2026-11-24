#include <memory>

#include "lug_memory.h"
#include "lug_vmcore.h"

#if LUG_DEBUG_LOG_GC
    #include <cstdio>
    #include "lug_debug.h"
#endif

namespace Lug::Memory {

/**
 * The 'god' function of reallocating memory on the heap. Any 
 * process in the VM that needs to allocate memory on the heap
 * will eventually worm its way back here. We route all memory
 * allocations to one place since this function holds two purposes.
 * While it does allocate/reallocate memory, it also regulates 
 * garbage collection for the VM.
 */
void* Reallocate(VMCore& vm, void* pointer, std::size_t oldSize, std::size_t newSize) {
    GC& gc = vm.gc;
    gc.bytesAllocated += newSize - oldSize;
    
    if (newSize > oldSize) {
    #if LUG_DEBUG_STRESS_GC
        vm.CollectGarbage();
    #endif
    }

    // Let's collect memory when the VM uses too much memory.
    // We have a threshold stored in the GC that is the number
    // of bytes allocated before we collect memory.
    if (newSize > oldSize && gc.bytesAllocated > gc.nextGC) {
        vm.CollectGarbage();
    }

    /*
        if oldSize == 0 and newSize != 0, allocate a new block
        if oldSize != 0 and newSize == 0, free the allocation 
        if oldSize != 0 and newSize < oldSize, shrink allocation  
        if oldSize != 0 and newSize > oldSize, grow allocation
    */
    if (newSize == 0) {
        std::free(pointer);
        return nullptr;
    }

    void* resized = std::realloc(pointer, newSize);

    // If allocation failed, we will throw a
    // bad_alloc error.
    if (resized == nullptr) {
        throw std::bad_alloc();
    }
    
    return resized;
}

}