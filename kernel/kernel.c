// myos/kernel/kernel.c

// headers
#include "idt.h"
#include "vga.h"
#include "types.h"

// Forward-declare functions from other files that we'll call.
void idt_install();

void kmain() {
    // Set up the Interrupt Descriptor Table
    idt_install();

    // Clear the screen to start with a blank slate
    clear_screen();
    
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