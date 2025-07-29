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
        // Add the USER flag to all kernel pages. Ring 0 can still access them.
        pte_t page = phys_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
        first_pt->entries[i] = page;
    }
    qemu_debug_string("PAGING_INIT: first_pt entries filled\n");

    // Put the newly created page table into the first entry of the page directory.
    // Add the USER flag here as well.
    kernel_directory->entries[0] = (pde_t)first_pt | PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER;
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