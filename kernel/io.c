// myos/kernel/io.c

#include "io.h"

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