// myos/include/kernel/paging.h

#ifndef PAGING_H
#define PAGING_H

#include <kernel/types.h>
#include <kernel/pmm.h>

// Define the structure of a Page Table Entry (PTE) and Page Directory Entry (PDE)
// We will use a simple uint32_t to represent them. The bits have specific meanings.
typedef uint32_t pte_t;
typedef uint32_t pde_t;

// Define some flags for our page table entries.
// These are based on the Intel SDM.
#define PAGING_FLAG_PRESENT       0x1 // Bit 0: Page is present in memory
#define PAGING_FLAG_RW            0x2 // Bit 1: Read/Write access
#define PAGING_FLAG_USER          0x4 // Bit 2: User-mode access

// A Page Table contains 1024 entries (4KB page size / 4-byte entry = 1024)
#define PAGE_TABLE_ENTRIES 1024
typedef struct {
    pte_t entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PMM_FRAME_SIZE))) page_table_t;

// A Page Directory also contains 1024 entries.
#define PAGE_DIRECTORY_ENTRIES 1024
typedef struct {
    pde_t entries[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PMM_FRAME_SIZE))) page_directory_t;

// Make the kernel's page directory globally accessible
extern page_directory_t* kernel_directory;

// This will be our main function to set up paging.
void paging_init();

#endif