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
    if (!kernel_directory) return; // Handle allocation failure
    
    // We will reserve the last entry in the page directory for temporary mapping.
    // This gives us a virtual address (0xFFC00000) that we can use to access
    // the physical memory of any page table we need to modify.
    page_table_t* temp_mapping = (page_table_t*)0xFFC00000;

    uint32_t virt_addr = 0;
    // We map 4 Page Tables to cover 16MB.
    for (int pd_idx = 0; pd_idx < 4; pd_idx++) {
        // Allocate a physical frame for the page table.
        page_table_t* new_table_phys = (page_table_t*)pmm_alloc_frame();
        if (!new_table_phys) return;

        // Put its physical address in the page directory.
        kernel_directory->entries[pd_idx] = (pde_t)new_table_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
        
        // Now, temporarily map this physical table to our virtual mapping address.
        kernel_directory->entries[1023] = (pde_t)new_table_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
        
        // Flush the TLB entry for our temporary mapping address.
        __asm__ __volatile__("invlpg (0xFFC00000)");

        // Now we can write to the page table using the `temp_mapping` virtual pointer.
        for (int pt_idx = 0; pt_idx < PAGE_TABLE_ENTRIES; pt_idx++) {
            temp_mapping->entries[pt_idx] = virt_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
            virt_addr += PMM_FRAME_SIZE;
        }
    }
    
    // Unmap the temporary page table entry before we finish.
    kernel_directory->entries[1023] = 0;
    __asm__ __volatile__("invlpg (0xFFC00000)");

    // Load the page directory into the CR3 register and enable paging.
    load_page_directory(kernel_directory);
    enable_paging();
    
    qemu_debug_string("Paging enabled.\n");
}