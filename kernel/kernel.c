// myos/kernel/kernel.c

// headers
#include "idt.h"
#include "vga.h"
#include "io.h"

// Helper for debug prints
static inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

void kmain() {
    outb(0xE9, '1'); // Checkpoint 1: kmain entered

    // Remap the PIC first to get the hardware into a stable state.
    pic_remap(0x20, 0x28); // Master PIC at 0x20, Slave at 0x28
    outb(0xE9, '2'); // Checkpoint 2: PIC remapped

    // Now, install our Interrupt Service Routines and load the IDT.
    idt_install();
    outb(0xE9, '3'); // Checkpoint 3: IDT installed and loaded

    // Clear the screen to start with a blank slate
    clear_screen();
    outb(0xE9, '4'); // Checkpoint 4: VGA cleared
    
    // Print a new message
    print_string("Kernel loaded successfully!");
    outb(0xE9, '5'); // Checkpoint 5: Message printed

    // Hang the CPU.
    while (1) {}
}