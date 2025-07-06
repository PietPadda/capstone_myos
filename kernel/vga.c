// myos/kernel/vga.c

#include "vga.h"
#include "io.h"
#include "types.h"

// Define the VGA buffer as a global, constant pointer to a volatile memory region.
volatile unsigned short* const VGA_BUFFER = (unsigned short*)0xB8000;

// Clear the screen by filling it with spaces
void clear_screen() {
    int i;
    for (int i = 0; i < 80 * 25; i++) {
        VGA_BUFFER[i] = (unsigned short)' ' | 0x0F00;
    }
}

// Updates the VGA cursor's position.
void update_cursor(int row, int col) {
    unsigned short position = (row * 80) + col;

    // Send the high byte of the cursor position
    port_byte_out(0x3D4, 0x0E);
    port_byte_out(0x3D5, (unsigned char)(position >> 8));
    // Send the low byte of the cursor position
    port_byte_out(0x3D4, 0x0F);
    port_byte_out(0x3D5, (unsigned char)(position & 0xFF));
}