// myos/kernel/cpu/tss.c

#include <kernel/cpu/tss.h>
#include <kernel/gdt.h>      // For the gdt_set_gate function
#include <kernel/string.h>   // For memset
#include <kernel/memory.h>   // For malloc
#include <kernel/pmm.h>      // For pmm_alloc_frame

// Our single, global TSS instance
struct tss_entry_struct tss_entry;

// Assembly function to load the TSS selector into the TR register
extern void tss_flush();

void tss_install() {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry);

    // Add the TSS descriptor to the GDT. 0x05 is the 6th entry (index 5).
    // 0x89 is the access byte for a 32-bit TSS.
    gdt_set_gate(5, base, limit, 0x89, 0x40);

    memset(&tss_entry, 0, sizeof(tss_entry));

    // explicitly tell the CPU there is no I/O map (x86 requirement)
    tss_entry.iomap_base = sizeof(tss_entry);

    // Allocate a dedicated 4KB page for the kernel stack.
    void* stack = pmm_alloc_frame();

    // Set the kernel stack segment and pointer
    tss_entry.ss0  = 0x10; // Kernel Data Segment selector
    // The stack grows downwards. The top must be aligned. We enforce 16-byte alignment
    tss_entry.esp0 = ((uint32_t)stack + PMM_FRAME_SIZE) & ~0xF; // use the allocated frame

    // Load the TSS selector into the CPU's Task Register (TR)
    tss_flush();
}