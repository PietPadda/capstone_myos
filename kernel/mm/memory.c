// myos/kernel/mm/memory.c

#include <kernel/memory.h>
#include <kernel/io.h>     // port_byte_out
#include <kernel/pmm.h>

// This symbol is defined by the linker script
extern uint32_t kernel_end;
extern uint32_t pmm_bitmap_size; // Make pmm_bitmap_size visible

// Define the header for each memory block
typedef struct block_header {
    size_t size;                // Size of the block
    struct block_header* next;  // Pointer to the next free block
} block_header_t;

// This pointer will be "bumped" forward as we allocate memory
static uint32_t heap_top;

// Head of our new free list
static block_header_t* free_list_head = NULL;

void init_memory() {
    // The heap starts right after the PMM's bitmap.
    // We calculate this based on the kernel_end symbol from the linker.
    heap_top = (uint32_t)&kernel_end + pmm_bitmap_size;

    // Align the heap_top to the next page boundary for safety.
    if (heap_top % PMM_FRAME_SIZE != 0) {
        heap_top = (heap_top / PMM_FRAME_SIZE + 1) * PMM_FRAME_SIZE;
    }
}

void* malloc(uint32_t size) {
    if (size == 0) {
        return NULL;
    }

    // This new implementation uses a pointer-to-a-pointer to make
    // list manipulation cleaner and to avoid the compiler bug.
    block_header_t** link = &free_list_head;
    while (*link) {
        block_header_t* current = *link;
        if (current->size >= size) {
            // Found a block. Unlink it from the list.
            *link = current->next;
            return (void*)(current + 1);
        }
        // Move to the 'next' pointer of the current block
        link = &current->next;
    }

    // Align the heap_top to a 4-byte boundary before allocating
    if (heap_top % 4 != 0) {
        heap_top = (heap_top + 3) & ~3;
    }

    // No suitable block found, allocate a new one from the top of the heap
    size_t total_size = sizeof(block_header_t) + size;
    block_header_t* new_block = (block_header_t*)heap_top;
    heap_top += total_size;

    new_block->size = size;
    new_block->next = NULL; // Not on the free list

    return (void*)(new_block + 1); // Return pointer to the user area

}

void free(void* ptr) {
    if (!ptr) {
        return; // Do nothing if a null pointer is freed
    }

    // Get the header of the block being freed
    block_header_t* header = (block_header_t*)ptr - 1;

    // Add it to the front of the free list
    header->next = free_list_head;
    free_list_head = header;
}
