// myos/kernel/mm/pmm.c

#include <kernel/pmm.h>
#include <kernel/types.h>
#include <kernel/string.h> // For memset
#include <kernel/debug.h>  // For qemu_debug_string

// This symbol is defined by the linker script
extern uint32_t kernel_end;

// A bitmap for tracking free physical memory frames.
uint32_t* pmm_bitmap = NULL; // points to array of bits
uint32_t pmm_total_frames = 0; // no of 4kb frames to manage
uint32_t pmm_bitmap_size = 0;

// Helper function to set a bit in the bitmap (mark a frame as used).
static inline void pmm_set_bit(uint32_t frame_idx) {
    uint32_t dword_idx = frame_idx / 32;
    uint32_t bit_idx = frame_idx % 32;
    pmm_bitmap[dword_idx] |= (1 << bit_idx);
}

// Helper function to clear a bit in the bitmap (mark a frame as free).
static inline void pmm_clear_bit(uint32_t frame_idx) {
    uint32_t dword_idx = frame_idx / 32;
    uint32_t bit_idx = frame_idx % 32;
    pmm_bitmap[dword_idx] &= ~(1 << bit_idx);
}

// Initializes the physical memory manager.
void pmm_init(uint32_t mem_size_bytes) {
    // Calculate the total number of 4KB frames in memory.
    pmm_total_frames = mem_size_bytes / PMM_FRAME_SIZE;

    // Calculate the size of the bitmap needed to track all frames.
    // Each byte in the bitmap tracks 8 frames, so we divide by 8.
    pmm_bitmap_size = pmm_total_frames / 8;
    // We must align this to a 4-byte boundary for our uint32_t* pointer.
    if (pmm_bitmap_size % 4 != 0) {
        pmm_bitmap_size = (pmm_bitmap_size / 4 + 1) * 4;
    }
    
    // The simplest place to put the bitmap is right after the kernel.
    // The 'kernel_end' symbol is provided by our linker script.
    pmm_bitmap = (uint32_t*)&kernel_end;

    // Mark all frames as initially available.
    memset(pmm_bitmap, 0, pmm_bitmap_size);

    // Calculate how many frames are used by the kernel itself, plus the PMM bitmap.
    // This is the highest memory address that is off-limits.
    uint32_t reserved_area_end = (uint32_t)&kernel_end + pmm_bitmap_size;
    uint32_t reserved_frames = (reserved_area_end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    // Mark all of these frames as "used" so we never allocate them.
    for (uint32_t i = 0; i < reserved_frames; i++) {
        pmm_set_bit(i);
    }
    
    qemu_debug_string("PMM: Initialized. Total frames: ");
    qemu_debug_hex(pmm_total_frames);
    qemu_debug_string(", Reserved frames: ");
    qemu_debug_hex(reserved_frames);
    qemu_debug_string("\n");
}

// Allocates a single 4KB frame of physical memory.
void* pmm_alloc_frame() {
    // Find the first free frame by scanning the bitmap.
    for (uint32_t dword = 0; dword < pmm_bitmap_size / 4; dword++) {
        // Optimization: If the dword is all 1s, all 32 frames are used. Skip it.
        if (pmm_bitmap[dword] != 0xFFFFFFFF) {
            for (int bit = 0; bit < 32; bit++) {
                // Check if the bit is 0 (meaning the frame is free).
                if (!(pmm_bitmap[dword] & (1 << bit))) {
                    uint32_t frame_idx = dword * 32 + bit;
                    pmm_set_bit(frame_idx);
                    return (void*)(frame_idx * PMM_FRAME_SIZE);
                }
            }
        }
    }
    
    // No free frames were found.
    return NULL; 
}

// Frees a previously allocated physical memory frame.
void pmm_free_frame(void* addr) {
    uint32_t frame_idx = (uint32_t)addr / PMM_FRAME_SIZE;
    pmm_clear_bit(frame_idx);
}

// Returns the first memory address available for use after the PMM bitmap.
void* pmm_get_free_addr() {
    // The bitmap is an array of uint32_t. We can get its end address
    // by taking the start address and adding its size in bytes.
    return (void*)((uint32_t)pmm_bitmap + pmm_bitmap_size);
}