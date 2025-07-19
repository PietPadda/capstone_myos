// myos/kernel/cpu/idt.c

#include <kernel/idt.h>

// Forward-declarations for our 32 ISR stubs in isr.asm
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3(); 
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7(); 
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11(); 
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15(); 
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19(); 
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23(); 
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27(); 
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();
extern void isr128(); // Our new syscall handler

// Forward-declarations for our 16 IRQ stubs in isr.asm
extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

// Declare our IDT with 256 entries.
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// This is our new, self-contained function to load the IDT.
static inline void idt_load_inline(struct idt_ptr_struct* idt_ptr) {
    __asm__ __volatile__("lidt %0" : : "m"(*idt_ptr));
}

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

// Initializes the IDT and loads it.
void idt_install() {
    // Set the IDT pointer struct's values
    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    // For now, clear the entire IDT by pointing all entries to nothing.
    // In later steps, we'll replace these with actual handlers.
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    // Use Trap Gate flags (0x8F) for CPU exceptions!
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8F);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8F);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8F);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8F);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8F);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8F);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8F);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8F);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8F);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8F);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8F);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8F);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8F);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8F);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8F);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8F);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8F);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8F);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8F);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8F);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8F);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8F);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8F);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8F);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8F);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8F);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8F);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8F);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8F);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8F);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8F);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8F);

    // Use Interrupt Gate flags (0x8E) for IRQs!
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // Set the gate for our system call interrupt 0x80
    // The flags 0xEE mean: Present, Ring 3, 32-bit Trap Gate
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    // Load the IDT using our new inline assembly function
    idt_load_inline(&idt_ptr);
}