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
    outb(0xE9, 'K'); // Checkpoint: kmain entered

    // Set up the IDT AND install the exception handlers
    /// Remap the PIC first to get the hardware into a stable state.
    pic_remap(0x20, 0x28); // Master PIC at 0x20, Slave at 0x28
    outb(0xE9, 'P'); // Checkpoint: PIC remapped

    // Now, install our Interrupt Service Routines and load the IDT.
    idt_install();
    outb(0xE9, 'I'); // Checkpoint: IDT installed

    // Clear the screen to start with a blank slate
    clear_screen();
    outb(0xE9, 'V'); // Checkpoint: VGA (clear_screen)
    
    // Print a new message
    char message[] = "Kernel Loaded! Code is now organized.";
    volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;
    int j = 0;

    // --- Print the message from the stack array ---
    while (message[j] != '\0') {
        vga_buffer[j] = (unsigned short)message[j] | 0x0F00;
        j++;
    }

    // Move the cursor to the end of the message
    update_cursor(0, j);

    // Hang the CPU.
    while (1) {}
}