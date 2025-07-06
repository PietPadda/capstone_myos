// myos/kernel/kernel.c

// headers
#include "idt.h"

// Forward declare our install function
void idt_install();

/**
 * Writes a byte to the specified I/O port.
 */
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

/**
 * Updates the VGA cursor's position.
 */
void update_cursor(int row, int col) {
    unsigned short position = (row * 80) + col;

    // Send the high byte of the cursor position
    port_byte_out(0x3D4, 0x0E);
    port_byte_out(0x3D5, (unsigned char)(position >> 8));
    // Send the low byte of the cursor position
    port_byte_out(0x3D4, 0x0F);
    port_byte_out(0x3D5, (unsigned char)(position & 0xFF));
}

void kmain() {
    // Install our Interrupt Descriptor Table
    idt_install();
    
    // The bootloader passes the VGA buffer address in EAX.
    volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;
    // Define the message as a local char array.
    // This places the string data on the stack, which we know works,
    // bypassing any potential linker issues with data sections.
    char message[] = "Kernel Loaded! Success!";
    int i = 0;
    int j = 0;

    // Clear the screen by filling it with spaces
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (unsigned short)' ' | 0x0F00;
    }

    // --- Print the message from the stack array ---
    while (message[j] != '\0') {
        vga_buffer[j] = (unsigned short)message[j] | 0x0F00;
        j++;
    }

    // --- Update the cursor to the position after the message ---
    update_cursor(0, j);

    // Hang the CPU.
    while (1) {}
}