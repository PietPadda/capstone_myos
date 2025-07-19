// myos/kernel/cpu/tss.c

#include <kernel/cpu/tss.h>
#include <kernel/gdt.h>      // For the gdt_set_gate function
#include <kernel/string.h>   // For memset

// The GDT helper function is in gdt.c, so we need to declare it
extern void gdt_set_gate(int32_t, uint32_t, uint32_t, uint8_t, uint8_t);

// Our single, global TSS instance
static struct tss_entry_struct tss_entry;

// Assembly function to load the TSS selector into the TR register
extern void tss_flush();

void tss_install() {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry);

    // Add the TSS descriptor to the GDT. 0x05 is the 6th entry (index 5).
    // 0x89 is the access byte for a 32-bit TSS.
    gdt_set_gate(5, base, limit, 0x89, 0x40);

    memset(&tss_entry, 0, sizeof(tss_entry));

    // Set the kernel stack segment and pointer
    tss_entry.ss0  = 0x10; // Kernel Data Segment selector
    tss_entry.esp0 = 0x0;  // This will be updated later

    // Load the TSS selector into the CPU's Task Register (TR)
    tss_flush();
}