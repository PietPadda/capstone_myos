// myos/kernel/mm/paging.c

#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/types.h>
#include <kernel/string.h> // For memset
#include <kernel/debug.h>

// A virtual address in the kernel's space that we reserve for temporary mappings.
// This must be an address that we know is not used for anything else.
#define TEMP_PAGETABLE_ADDR 0xFFBFF000
#define TEMP_PAGEDIR_ADDR   0xFFBFE000 // A different, unused virtual page

// our assembly functions
extern void load_page_directory(page_directory_t* dir);
extern void enable_paging();

// The kernel's page directory, now globally visible.
page_directory_t* kernel_directory = NULL;
extern task_struct_t* current_task;

// A virtual address pointer to the page tables of the current page directory.
#define CURRENT_PAGE_TABLES ((page_table_t*)0xFFC00000)

// A virtual address pointer to the current page directory, thanks to our recursive mapping.
#define CURRENT_PAGE_DIR ((page_directory_t*)0xFFFFF000)


// This function sets up and enables paging.
void paging_init() {
    //qemu_debug_string("PAGING_INIT: start\n");

    kernel_directory = (page_directory_t*)pmm_alloc_frame();
    if (!kernel_directory) {
        qemu_debug_string("PAGING_INIT: PANIC! no frame for page directory\n");
        return;
    }
    //qemu_debug_string("PAGING_INIT: kernel_directory allocated\n");
    memset(kernel_directory, 0, sizeof(page_directory_t));
    //qemu_debug_string("PAGING_INIT: kernel_directory zeroed\n");

    // We will identity map the first 4MB of memory.
    page_table_t* first_pt = (page_table_t*)pmm_alloc_frame();
    if (!first_pt) {
        qemu_debug_string("PAGING_INIT: PANIC! no frame for page table\n");
        return;
    }
    //qemu_debug_string("PAGING_INIT: first_pt allocated\n");
    memset(first_pt, 0, sizeof(page_table_t));
    //qemu_debug_string("PAGING_INIT: first_pt zeroed\n");

    // Loop through all 1024 entries in the page table to map 4MB.
    for (int i = 0; i < 1024; i++) {
        uint32_t phys_addr = i * 0x1000;
        // The USER flag is no longer needed here, as the PDE provides protection.
        pte_t page = phys_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
        first_pt->entries[i] = page;
    }
    //qemu_debug_string("PAGING_INIT: first_pt entries filled\n");

    // Put the newly created page table into the first entry of the page directory.
    // The USER flag is NOT set, protecting the kernel from user-mode access.
    kernel_directory->entries[0] = (pde_t)first_pt | PAGING_FLAG_PRESENT | PAGING_FLAG_RW ;
    //qemu_debug_string("PAGING_INIT: page directory entry [0] set\n");

    // Add the recursive mapping.
    // The last entry of the page directory is made to point to the directory's physical address.
    uint32_t page_dir_phys_addr = (uint32_t)kernel_directory;
    kernel_directory->entries[1023] = page_dir_phys_addr | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    //emu_debug_string("PAGING_INIT: Recursive mapping set for entry 1023.\n");

    load_page_directory(kernel_directory);
    //qemu_debug_string("PAGING_INIT: CR3 loaded with page directory address\n");

    enable_paging();
    //qemu_debug_string("PAGING_INIT: Paging bit set in CR0. MMU is now active.\n");
}

// Clones a page directory and its tables.
page_directory_t* paging_clone_directory(page_directory_t* src_phys) {
    //qemu_debug_string("PAGING: clone_directory started.\n");
    page_directory_t* new_dir_phys = (page_directory_t*)pmm_alloc_frame();

    // error checking
    if (!new_dir_phys) {
        qemu_debug_string("PAGING: PANIC! No frame for new directory.\n");
        return NULL;
    }
    //qemu_debug_string("PAGING: new_dir_phys allocated.\n");

    // Temporarily map the new directory so we can write to it safely.
    paging_map_page(CURRENT_PAGE_DIR, TEMP_PAGEDIR_ADDR, (uint32_t)new_dir_phys, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);

    // We can safely write to new_dir_phys because it's in identity-mapped low memory.
    page_directory_t* new_dir_virt = (page_directory_t*)TEMP_PAGEDIR_ADDR;
    //qemu_debug_string("PAGING: before zero out the new directory.\n");

    // Zero out the new directory.
    memset(new_dir_virt, 0, sizeof(page_directory_t));
   //qemu_debug_string("PAGING: after zero out the new directory.\n");

    // Read from the source directory using the reliable recursive mapping.
    // This ensures we are reading from the true, active page directory.
    page_directory_t* src_virt = CURRENT_PAGE_DIR;

    // Copy the mapping for the first 4MB, which contains the kernel.
    new_dir_virt->entries[0] = src_virt->entries[0];

    //qemu_debug_string("PAGING: before copy kernel space entries.\n");
    // Copy kernel-space entries (upper 1GB, entries 768-1022).
    for (int i = 768; i < 1023; i++) {
        if (src_virt->entries[i] & PAGING_FLAG_PRESENT) {
            new_dir_virt->entries[i] = src_virt->entries[i];
        }
    }
    //qemu_debug_string("PAGING: Kernel entries copied.\n");

    // Set the recursive mapping for the new directory to point to itself.
    new_dir_virt->entries[1023] = (uint32_t)new_dir_phys | PAGING_FLAG_PRESENT | PAGING_FLAG_RW;
    //qemu_debug_string("PAGING: Recursive mapping set for new directory.\n");

        // Unmap the temporary page.
    paging_map_page(CURRENT_PAGE_DIR, TEMP_PAGEDIR_ADDR, 0, 0);

    //qemu_debug_string("PAGING: clone_directory finished successfully.\n");
    return new_dir_phys;
}

// It frees the page tables and pages of a given directory.
void paging_free_directory(page_directory_t* dir_phys) {
    // err check
    if (!dir_phys) return;

    // Temporarily map the directory we want to free into our current address space.
    paging_map_page(CURRENT_PAGE_DIR, TEMP_PAGEDIR_ADDR, (uint32_t)dir_phys, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
    page_directory_t* dir_virt = (page_directory_t*)TEMP_PAGEDIR_ADDR;
    
    // Free all user-space pages and page tables (entries 1 to 767).
    // We start at 1 because entry 0 maps the kernel's low memory, which is shared
    // and should never be freed by a user process.
    for (int i = 1; i < 768; i++) {
        // Access the PDE using the safe VIRTUAL address.
        pde_t pde = dir_virt->entries[i];

        if (pde & PAGING_FLAG_PRESENT) {
            // Get the PHYSICAL address of the page table from the directory entry.
            page_table_t* pt_phys = (page_table_t*)(pde & ~0xFFF);

            // To safely read the contents of this page table, we must temporarily
            // map it into our CURRENT (kernel) address space.
            paging_map_page(kernel_directory, TEMP_PAGETABLE_ADDR, (uint32_t)pt_phys, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);

            // Now we can access the page table through a safe virtual pointer.
            page_table_t* pt_virt = (page_table_t*)TEMP_PAGETABLE_ADDR;

            // Iterate through the page table and free every physical frame it points to.
            for (int j = 0; j < 1024; j++) {
                if (pt_virt->entries[j] & PAGING_FLAG_PRESENT) {
                    pmm_free_frame((void*)(pt_virt->entries[j] & ~0xFFF));
                }
            }

            // After we're done with this page table, unmap it from our temporary slot.
            paging_map_page(kernel_directory, TEMP_PAGETABLE_ADDR, 0, 0);

            // And finally, free the physical frame that held the page table itself.
            pmm_free_frame(pt_phys);
        }
    }

    // Unmap the directory itself.
    paging_map_page(CURRENT_PAGE_DIR, TEMP_PAGEDIR_ADDR, 0, 0);

    // Finally, free the physical frame that held the page directory.
    pmm_free_frame(dir_phys);
    
    // IMPORTANT: The caller (sys_exit) is now responsible for immediately
    // switching to a new, valid page directory because the one we were just
    // using is now gone.
}

// Revert to the original version that correctly uses recursive mapping for the active directory.
pte_t* paging_get_page(page_directory_t* dir, uint32_t virt_addr, bool create, uint32_t flags) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    // Use the magic virtual address for the currently active page directory.
    if (!(CURRENT_PAGE_DIR->entries[pd_idx] & PAGING_FLAG_PRESENT)) {
        if (create) {
            uint32_t new_table_phys = (uint32_t)pmm_alloc_frame();
            if (!new_table_phys) {
                return NULL; // Out of memory
            }
            // We use the physical address here because it's in low memory
            // and identity-mapped.
            memset((void*)new_table_phys, 0, sizeof(page_table_t));
            CURRENT_PAGE_DIR->entries[pd_idx] = new_table_phys | (flags & 0x7);

            // Invalidate the TLB for the page table's virtual address
            __asm__ __volatile__("invlpg (%0)" : : "b"(&CURRENT_PAGE_TABLES[pd_idx]) : "memory");
        } else {
            return NULL;
        }
    }

    // Use the magic virtual address "window" to access the page table.
    page_table_t* table = &CURRENT_PAGE_TABLES[pd_idx];
    return &table->entries[pt_idx];
}

// Revert this function as well to match the stable paging_get_page.
void paging_map_page(page_directory_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    pte_t* pte = paging_get_page(dir, virt_addr, true, flags);
    if (pte) {
        *pte = phys_addr | flags;
        __asm__ __volatile__("invlpg (%0)" : : "b"(virt_addr) : "memory");
    }
}

void paging_switch_directory(page_directory_t* dir) {
    //qemu_debug_string("PAGING: state dump inside paging_swing_directory\n\n");
    //qemu_debug_cpu_state(&current_task->cpu_state);
    //qemu_debug_string("\n\n");
    //qemu_debug_memdump(dir, sizeof(page_directory_t));
    //qemu_debug_string("\n\n");
    //qemu_debug_string("PAGING: before loading page dir\n");
    load_page_directory(dir);
    //qemu_debug_string("PAGING: after loading page dir\n");
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