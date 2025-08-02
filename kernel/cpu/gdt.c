// myos/kernel/cpu/gdt.c

#include <kernel/gdt.h>
#include <kernel/types.h>

// Tell the C compiler that our assembly function exists elsewhere.
extern void gdt_flush(struct gdt_ptr_struct* gdt_ptr);

// GDT entry structure
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// GDT pointer structure
struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Our GDT will now have 6 entries
static struct gdt_entry_struct gdt_entries[6];
static struct gdt_ptr_struct   gdt_ptr;

// Replaced inline with assembly version
/*
// Assembly inline function to load the GDT
static inline void gdt_flush_inline(struct gdt_ptr_struct* gdt_ptr) {
    __asm__ __volatile__(
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t" // 0x10 is our kernel data segment selector
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "ljmp $0x08, $.flush\n\t" // 0x08 is the code segment selector
        ".flush:\n\t"
        : : "r"(gdt_ptr) : "ax");
}
*/

// Helper function to create a GDT entry
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access      = access;
}

// Main function to install the GDT with 6 Entries
void gdt_install() {
    gdt_ptr.limit = (sizeof(struct gdt_entry_struct) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;
    
    // Kernel Segments (Ring 0)
    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment (Ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment (Ring 0)

    // User Segments (Ring 3)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data

    // The TSS entry will be set up by tss_install()

    // Load our new GDT using the safe assembly function.
    gdt_flush(&gdt_ptr);
}