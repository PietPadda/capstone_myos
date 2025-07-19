// myos/kernel/drivers/io.c

#include <kernel/io.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

// Writes a byte to the specified I/O port.
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Reads a byte from the specified I/O port.
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

unsigned short port_word_in(unsigned short port) {
    unsigned short result;
    __asm__("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

/**
 * Remaps the PIC to use non-conflicting interrupt vectors.
 * @param offset1 Vector offset for master PIC (e.g., 0x20)
 * @param offset2 Vector offset for slave PIC (e.g., 0x28)
 */
void pic_remap(int offset1, int offset2) {
    // Save current masks
    unsigned char a1 = port_byte_in(PIC1_DATA);
    unsigned char a2 = port_byte_in(PIC2_DATA);

    // ICW1: Start the initialization sequence
    port_byte_out(PIC1_CMD, 0x11);
    port_byte_out(PIC2_CMD, 0x11);

    // ICW2: Set the vector offsets
    port_byte_out(PIC1_DATA, offset1);
    port_byte_out(PIC2_DATA, offset2);

    // ICW3: Configure chaining between master and slave
    port_byte_out(PIC1_DATA, 4);  // Tell Master PIC there is a slave at IRQ2
    port_byte_out(PIC2_DATA, 2);  // Tell Slave PIC its cascade identity

    // ICW4: Set 8086 mode
    port_byte_out(PIC1_DATA, 0x01);
    port_byte_out(PIC2_DATA, 0x01);

    // Restore saved masks
    port_byte_out(PIC1_DATA, a1);
    port_byte_out(PIC2_DATA, a2);
}