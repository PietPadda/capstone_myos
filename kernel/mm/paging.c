// myos/kernel/mm/paging.c

#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/types.h>
#include <kernel/string.h> // For memset
#include <kernel/debug.h>

// our assembly functions
extern void load_page_directory(page_directory_t* dir);
extern void enable_paging();

// The kernel's page directory, now globally visible.
page_directory_t* kernel_directory = NULL;

// This function sets up and enables paging.
void paging_init() {
    qemu_debug_string("PAGING_INIT: start\n");

    kernel_directory = (page_directory_t*)pmm_alloc_frame();
    if (!kernel_directory) {
        qemu_debug_string("PAGING_INIT: PANIC! no frame for page directory\n");
        return;
    }
    qemu_debug_string("PAGING_INIT: kernel_directory allocated\n");
    memset(kernel_directory, 0, sizeof(page_directory_t));
    qemu_debug_string("PAGING_INIT: kernel_directory zeroed\n");

    // We will identity map the first 4MB of memory.
    page_table_t* first_pt = (page_table_t*)pmm_alloc_frame();
    if (!first_pt) {
        qemu_debug_string("PAGING_INIT: PANIC! no frame for page table\n");
        return;
    }
    qemu_debug_string("PAGING_INIT: first_pt allocated\n");
    memset(first_pt, 0, sizeof(page_table_t));
    qemu_debug_string("PAGING_INIT: first_pt zeroed\n");

    // Loop through all 1024 entries in the page table to map 4MB.
    for (int i = 0; i < 1024; i++) {
        uint32_t phys_addr = i * 0x1000;
        // The USER flag is no longer needed here, as the PDE provides protection.
        pte_t page = phys_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
        first_pt->entries[i] = page;
    }
    qemu_debug_string("PAGING_INIT: first_pt entries filled\n");

    // Put the newly created page table into the first entry of the page directory.
    // The USER flag is NOT set, protecting the kernel from user-mode access.
    kernel_directory->entries[0] = (pde_t)first_pt | PAGING_FLAG_PRESENT | PAGING_FLAG_RW ;
    qemu_debug_string("PAGING_INIT: page directory entry [0] set\n");

    // Add the recursive mapping.
    // The last entry of the page directory is made to point to the directory's physical address.
    uint32_t page_dir_phys_addr = (uint32_t)kernel_directory;
    kernel_directory->entries[1023] = page_dir_phys_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    qemu_debug_string("PAGING_INIT: Recursive mapping set for entry 1023.\n");

    load_page_directory(kernel_directory);
    qemu_debug_string("PAGING_INIT: CR3 loaded with page directory address\n");

    enable_paging();
    qemu_debug_string("PAGING_INIT: Paging bit set in CR0. MMU is now active.\n");
}

// Clones a page directory and its tables.
page_directory_t* paging_clone_directory(page_directory_t* src) {
    // Allocate a new frame for the new page directory.
    page_directory_t* new_dir = (page_directory_t*)pmm_alloc_frame();
    if (!new_dir) {
        return NULL;
    }
    memset(new_dir, 0, sizeof(page_directory_t));

    // Copy the kernel-space entries from the source directory.
    // The kernel space is typically in the upper 1GB (last 256 entries).
    // For now, we'll copy all existing entries, as we only have kernel mappings.
    for (int i = 0; i < 1024; i++) {
        if (src->entries[i] & PAGING_FLAG_PRESENT) {
            new_dir->entries[i] = src->entries[i];
        }
    }

    // Add the recursive mapping for the new directory.
    new_dir->entries[1023] = (uint32_t)new_dir | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    
    return new_dir;
}

// Frees a page directory.
// (For now, it only frees the directory frame itself, not the tables).
void paging_free_directory(page_directory_t* dir) {
    if (dir) {
        pmm_free_frame(dir);
    }
}

// A virtual address pointer to the page tables of the current page directory.
#define CURRENT_PAGE_TABLES ((page_table_t*)0xFFC00000)

// Finds the Page Table Entry (PTE) for a given virtual address.
// If a page table doesn't exist and `create` is true, it will be allocated.
pte_t* paging_get_page(page_directory_t* dir, uint32_t virt_addr, bool create) {
    // The virtual address is split into a 10-bit directory index and a 10-bit table index.
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    // Check if the page table is present in the target directory
    if (!(dir->entries[pd_idx] & PAGING_FLAG_PRESENT)) {
        if (create) {
            // The page table doesn't exist, so we allocate a new physical frame for it.
            uint32_t new_table_phys = (uint32_t)pmm_alloc_frame();

            // err check
            if (!new_table_phys) {
                return NULL; // Out of memory
            }
            // IMPORTANT: Initialize the new page table to all zeros!
            memset((void*)new_table_phys, 0, sizeof(page_table_t));

            // Map the new table into the directory with USER flag
            dir->entries[pd_idx] = new_table_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;

            // We just modified a PDE. We must invalidate the TLB entry for the
            // corresponding page table's virtual address in our recursive mapping.
            __asm__ __volatile__("invlpg (%0)" : : "b"(&CURRENT_PAGE_TABLES[pd_idx]) : "memory");
        } else {
            return NULL; // Page table doesn't exist and we're not allowed to create it.
        }
    }

    // Now, we can access the page table via our recursive mapping "window".
    // CURRENT_PAGE_TABLES gives us a virtual address to the array of all page tables.
    // pd_idx selects the correct page table from that array.

    // Now, we can safely access the page table.
    page_table_t* table = &CURRENT_PAGE_TABLES[pd_idx];
    
    // Return a pointer to the actual Page Table Entry.
    return &table->entries[pt_idx];
}


// Maps a virtual address to a physical address in the given page directory.
void paging_map_page(page_directory_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    // Get the page table entry for the virtual address, creating tables if necessary.
    pte_t* pte = paging_get_page(dir, virt_addr, true);
    if (pte) {
        // Set the entry to point to the physical frame with the given flags.
        *pte = phys_addr | flags;

        // Invalidate the TLB entry for this virtual address to ensure the change takes effect.
        __asm__ __volatile__("invlpg (%0)" : : "b"(virt_addr) : "memory");
    }
}

void paging_switch_directory(page_directory_t* dir) {
    load_page_directory(dir);
}