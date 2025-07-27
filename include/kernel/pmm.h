// myos/include/kernel/pmm.h

#ifndef PMM_H
#define PMM_H

#include <kernel/types.h>

#define PMM_FRAME_SIZE 4096 // We'll use 4KB frames

// Initializes the physical memory manager.
// mem_size_bytes: The total amount of physical memory available.
void pmm_init(uint32_t mem_size_bytes);

// Allocates a single 4KB frame of physical memory.
// Returns the physical address of the allocated frame, or 0 if no frames are free.
void* pmm_alloc_frame();

// Frees a previously allocated physical memory frame.
// addr: The physical address of the frame to free.
void pmm_free_frame(void* addr);

// Returns the first memory address available for use after the PMM bitmap.
void* pmm_get_free_addr();

#endif