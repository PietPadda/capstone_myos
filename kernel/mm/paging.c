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
    // Allocate a frame for our kernel's page directory.
    kernel_directory = (page_directory_t*)pmm_alloc_frame();
    if (!kernel_directory) {
        // Handle allocation failure
        return;
    }
    memset(kernel_directory, 0, sizeof(page_directory_t));

    // We need to map 16MB of memory, which requires 4 page tables (4MB each).
    // Pre-allocate and set up the Page Directory Entries for these tables.
    for (int i = 0; i < 4; i++) {
        page_table_t* table = (page_table_t*)pmm_alloc_frame();
        if (!table) {
            // Handle allocation failure
            return;
        }
        memset(table, 0, sizeof(page_table_t));
        // We set Present, Read/Write, and User flags for each page table.
        kernel_directory->entries[i] = (pde_t)table | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
    }

    // Now, loop through the tables and create the 1-to-1 (identity) mapping.
    uint32_t virt_addr = 0;
    for (int pd_idx = 0; pd_idx < 4; pd_idx++) {
        page_table_t* table = (page_table_t*)(kernel_directory->entries[pd_idx] & ~0xFFF);
        for (int pt_idx = 0; pt_idx < PAGE_TABLE_ENTRIES; pt_idx++) {
            table->entries[pt_idx] = virt_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
            virt_addr += PMM_FRAME_SIZE;
        }
    }

    // Load the page directory into the CR3 register and enable paging.
    load_page_directory(kernel_directory);
    enable_paging();
    
    qemu_debug_string("Paging enabled.\n");
}