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

// A virtual address pointer to the page tables of the current page directory.
#define CURRENT_PAGE_TABLES ((page_table_t*)0xFFC00000)

// A virtual address pointer to the current page directory, thanks to our recursive mapping.
#define CURRENT_PAGE_DIR ((page_directory_t*)0xFFFFF000)


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
page_directory_t* paging_clone_directory(page_directory_t* src_phys) {
    qemu_debug_string("PAGING: clone_directory started.\n");
    page_directory_t* new_dir_phys = (page_directory_t*)pmm_alloc_frame();
    if (!new_dir_phys) {
        qemu_debug_string("PAGING: PANIC! No frame for new directory.\n");
        return NULL;
    }
    qemu_debug_string("PAGING: new_dir_phys allocated.\n");

    // Because the first 4MB of memory are identity-mapped, and our PMM allocates
    // from a region well within that (e.g., after the kernel), we can simply
    // use the physical addresses as virtual addresses to access the contents
    // of the page directories. This avoids complex and unsafe temporary mappings.
    page_directory_t* src_virt = src_phys;
    page_directory_t* new_dir_virt = new_dir_phys;

    // Zero out the new directory.
    memset(new_dir_virt, 0, sizeof(page_directory_t));

    // Copy kernel-space entries (upper 1GB).
    for (int i = 768; i < 1023; i++) {
        if (src_virt->entries[i] & PAGING_FLAG_PRESENT) {
            new_dir_virt->entries[i] = src_virt->entries[i];
        }
    }
    qemu_debug_string("PAGING: Kernel entries copied.\n");

    // Set the recursive mapping for the new directory to point to itself.
    new_dir_virt->entries[1023] = (uint32_t)new_dir_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    qemu_debug_string("PAGING: Recursive mapping set for new directory.\n");

    qemu_debug_string("PAGING: clone_directory finished successfully.\n");
    return new_dir_phys;
}

// Frees all user-space pages and page tables for a given page directory.
void paging_free_directory(page_directory_t* dir_phys) {
    // err check
    if (!dir_phys) return;

    // Map the directory to be freed so we can read its entries.
    pde_t* pde = &CURRENT_PAGE_DIR->entries[1022];
    uint32_t temp_vaddr = (uint32_t)&CURRENT_PAGE_TABLES[1022];
    *pde = (pde_t)dir_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
     __asm__ __volatile__("invlpg (%0)" : : "b"(temp_vaddr));
    page_directory_t* dir_virt = (page_directory_t*)temp_vaddr;
    
    // Iterate through all page directory entries (0-767)
    for (int i = 0; i < 768; i++) { // Iterate user-space PDEs
        if (dir_virt->entries[i] & PAGING_FLAG_PRESENT) {
            page_table_t* pt_phys = (page_table_t*)(dir_virt->entries[i] & ~0xFFF);
            
            // Temporarily map the page table to free its frames
            pde_t* pde2 = &CURRENT_PAGE_DIR->entries[1021];
            uint32_t temp_vaddr2 = (uint32_t)&CURRENT_PAGE_TABLES[1021];
            *pde2 = (pde_t)pt_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
            __asm__ __volatile__("invlpg (%0)" : : "b"(temp_vaddr2));
            page_table_t* pt_virt = (page_table_t*)temp_vaddr2;

            for (int j = 0; j < 1024; j++) {
                if (pt_virt->entries[j] & PAGING_FLAG_PRESENT) {
                    pmm_free_frame((void*)(pt_virt->entries[j] & ~0xFFF));
                }
            }
            *pde2 = 0; // Unmap page table
            __asm__ __volatile__("invlpg (%0)" : : "b"(temp_vaddr2));
            pmm_free_frame(pt_phys);
        }
    }

    // Unmap the page directory itself before freeing it.
    *pde = 0;
    __asm__ __volatile__("invlpg (%0)" : : "b"(temp_vaddr));
    pmm_free_frame(dir_phys);
}

// Finds the Page Table Entry (PTE) for a given virtual address.
// If a page table doesn't exist and `create` is true, it will be allocated.
pte_t* paging_get_page(page_directory_t* dir, uint32_t virt_addr, bool create, uint32_t flags) {
    // The virtual address is split into a 10-bit directory index and a 10-bit table index.
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    // We operate on the CURRENTLY ACTIVE page directory, which `exec_program`
    // has already switched to. We use the magic virtual address for this.
    if (!(CURRENT_PAGE_DIR->entries[pd_idx] & PAGING_FLAG_PRESENT)) {
        if (create) {
            // The page table doesn't exist, so we allocate a new physical frame for it.
            uint32_t new_table_phys = (uint32_t)pmm_alloc_frame();

            // err check
            if (!new_table_phys) {
                return NULL; // Out of memory
            }
            
            // Get a virtual pointer to the new physical table to clear it.
            // We can do this by temporarily mapping it.
            page_table_t* table_virt = (page_table_t*)0xFFBFF000; // Use a dedicated temp virtual address
            CURRENT_PAGE_DIR->entries[1022] = new_table_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
            __asm__ __volatile__("invlpg (%0)" : : "b"(table_virt));
            memset(table_virt, 0, sizeof(page_table_t));
            CURRENT_PAGE_DIR->entries[1022] = 0; // Unmap
             __asm__ __volatile__("invlpg (%0)" : : "b"(table_virt));


            // We only care about the top-level flags (USER, RW, PRESENT) for the PDE.
            CURRENT_PAGE_DIR->entries[pd_idx] = new_table_phys | (flags & 0x7);

            // Invalidate the TLB for the page table's recursive mapping address
            __asm__ __volatile__("invlpg (%0)" : : "b"(&CURRENT_PAGE_TABLES[pd_idx]) : "memory");
        } else {
            return NULL; // Page table doesn't exist and we're not allowed to create it.
        }
    }

    // Now, we can safely access the page table via our recursive mapping "window".
    page_table_t* table = &CURRENT_PAGE_TABLES[pd_idx];
    
    // Return a pointer to the actual Page Table Entry.
    return &table->entries[pt_idx];
}


// Maps a virtual address to a physical address in the given page directory.
void paging_map_page(page_directory_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    // Pass the flags through to the creation logic.
    pte_t* pte = paging_get_page(dir, virt_addr, true, flags);
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

// Dumps debug information about the PDE and PTE for a given virtual address.
void paging_dump_entry_for_addr(uint32_t virt_addr) {
    // Disable interrupts to ensure the paging structures aren't changed while we read them.
    __asm__ __volatile__("cli");

    qemu_debug_string("-- PAGING DUMP for VA: ");
    qemu_debug_hex(virt_addr);
    qemu_debug_string(" --\n");

    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    pde_t pde = CURRENT_PAGE_DIR->entries[pd_idx];

    qemu_debug_string("  PDE index: ");
    qemu_debug_hex(pd_idx);
    qemu_debug_string("   PDE value: ");
    qemu_debug_hex(pde);
    qemu_debug_string(" [");
    if (pde & PAGING_FLAG_PRESENT) qemu_debug_string("P ");
    if (pde & PAGING_FLAG_RW) qemu_debug_string("RW ");
    if (pde & PAGING_FLAG_USER) qemu_debug_string("U ");
    qemu_debug_string("]\n");

    if (pde & PAGING_FLAG_PRESENT) {
        pte_t pte = CURRENT_PAGE_TABLES[pd_idx].entries[pt_idx];
        qemu_debug_string("  PTE index: ");
        qemu_debug_hex(pt_idx);
        qemu_debug_string("   PTE value: ");
        qemu_debug_hex(pte);
        qemu_debug_string(" [");
        if (pte & PAGING_FLAG_PRESENT) qemu_debug_string("P ");
        if (pte & PAGING_FLAG_RW) qemu_debug_string("RW ");
        if (pte & PAGING_FLAG_USER) qemu_debug_string("U ");
        qemu_debug_string("] -> maps to physical: ");
        qemu_debug_hex(pte & ~0xFFF);
        qemu_debug_string("\n");
    }
    __asm__ __volatile__("sti");
}