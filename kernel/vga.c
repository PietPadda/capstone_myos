// myos/kernel/vga.c

#include "vga.h"
#include "io.h"
#include "types.h"

// Define a static cursor position
static int cursor_row = 0;
static int cursor_col = 0;

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

void print_char(char c) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = (VGA_BUFFER[(cursor_row * 80) + cursor_col] & 0xFF00) | c;
        cursor_col++;
    }

    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
    }
    update_cursor(cursor_row, cursor_col);
}

void print_string(const char* str) {
    int i = 0;
    while (str[i] != '\0') {
        print_char(str[i]);
        i++;
    }
}

void print_hex(uint32_t n) {
    print_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint32_t nibble = (n >> i) & 0xF;
        nibble += nibble > 9 ? 0x37 : 0x30;
        print_char((char)nibble);
    }
}