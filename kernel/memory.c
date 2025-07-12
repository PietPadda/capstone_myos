// myos/kernel/memory.c

#include "memory.h"

// This symbol is defined by the linker script
extern uint32_t kernel_end;

// This pointer will be "bumped" forward as we allocate memory
static uint32_t heap_top;

void init_memory() {
    // The heap starts right where the kernel binary ends
    heap_top = (uint32_t)&kernel_end;
}

void* malloc(uint32_t size) {
        // Check if the current heap top is aligned to a 4-byte boundary.
    if (heap_top % 4 != 0) {
        // If not, align it by "bumping" it up to the next multiple of 4.
        // This is a common bit-twiddling trick for alignment.
        heap_top = (heap_top + 3) & ~3;
    }
    
    // Save the current top of the heap, this is the address we will return
    uint32_t old_top = heap_top;

    // "Bump" the pointer forward by the requested size
    heap_top += size;

    // Return the start of the newly allocated block
    return (void*)old_top;
}