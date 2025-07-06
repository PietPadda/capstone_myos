// myos/kernel/idt.c

#include "idt.h"

// Declare our IDT with 256 entries.
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// This is the assembly function.
// It's defined in another file, so we use 'extern'.
extern void idt_load(struct idt_ptr_struct* idt_ptr);

/**
 * Sets a single gate (entry) in the IDT.
 * @param num The interrupt number (0-255).
 * @param base The address of the interrupt handler function.
 * @param selector The GDT code segment selector to use.
 * @param flags The flags for this gate (e.g., present, priority).
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt_entries[num].base_low    = base & 0xFFFF;
    idt_entries[num].base_high   = (base >> 16) & 0xFFFF;
    idt_entries[num].selector    = selector;
    idt_entries[num].always0     = 0;
    // We must uncomment the OR below when we add the interrupt handlers.
    idt_entries[num].flags       = flags /* | 0x60 */;
}

/**
 * Initializes the IDT and loads it.
 */
void idt_install() {
    // Set the IDT pointer struct's values
    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    // For now, clear the entire IDT by pointing all entries to nothing.
    // In later steps, we'll replace these with actual handlers.
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Load the IDT pointer using our assembly routine.
    idt_load(&idt_ptr);
}