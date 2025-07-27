// myos/kernel/mm/paging.c

#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/types.h>
#include <kernel/string.h> // For memset
#include <kernel/debug.h>

// our assembly functions
extern void load_page_directory(page_directory_t* dir);
extern void enable_paging();

// The kernel's page directory.
page_directory_t* kernel_directory = NULL;

// This function sets up and enables paging.
void paging_init() {
    // Create the Identity Map
    // Allocate a frame for our kernel's page directory.
    // pmm_alloc_frame() returns a 4KB-aligned address.
    kernel_directory = (page_directory_t*)pmm_alloc_frame();
    if (!kernel_directory) {
        qemu_debug_string("Paging Error: Failed to allocate kernel directory.\n");
        return;
    }
    // Clear the allocated memory.
    memset(kernel_directory, 0, sizeof(page_directory_t));

    // We will now create an identity map for the first 16MB of memory.
    // This covers all our kernel code, data, and available memory for now.
    for (uint32_t addr = 0; addr < 16 * 1024 * 1024; addr += PMM_FRAME_SIZE) {
        // Calculate the indices for the page directory and page table.
        uint32_t pd_idx = addr / (PAGE_TABLE_ENTRIES * PMM_FRAME_SIZE);
        uint32_t pt_idx = (addr / PMM_FRAME_SIZE) % PAGE_TABLE_ENTRIES;

        // Check if the page table for this address has been created yet.
        if (kernel_directory->entries[pd_idx] == 0) {
            // Allocate a new frame for the page table.
            page_table_t* new_table = (page_table_t*)pmm_alloc_frame();
            if (!new_table) {
                qemu_debug_string("Paging Error: Failed to allocate page table.\n");
                return;
            }
            memset(new_table, 0, sizeof(page_table_t));

            // Set the entry in the page directory.
            // We use the physical address of the table and set the Present and R/W flags.
            kernel_directory->entries[pd_idx] = (pde_t)new_table | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
        }

        // Get the page table from the directory entry.
        page_table_t* table = (page_table_t*)(kernel_directory->entries[pd_idx] & ~0xFFF);
        
        // Set the entry in the page table to map this virtual page to the same physical frame.
        table->entries[pt_idx] = addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    }

    // We will now call the assembly stubs after creating the identity map.
    // Load the page directory into the CR3 register and enable paging.
    load_page_directory(kernel_directory);
    enable_paging();
    
    qemu_debug_string("Paging enabled.\n");
}