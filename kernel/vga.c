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
    cursor_row = 0;
    cursor_col = 0;
    update_cursor(cursor_row, cursor_col);
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
    // Handle backspace
    if (c == '\b') {
        // Only backspace if the cursor isn't at the very beginning.
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = 79; // Move to the end of the previous line.
        }
        // Write a blank space to the current cursor position to 'erase' the char.
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = ' ' | 0x0F00;

    // Handle newline
    } else if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        // Handle a normal character.
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = (VGA_BUFFER[(cursor_row * 80) + cursor_col] & 0xFF00) | c;
        cursor_col++;
    }

    // If we're at the end of the line, wrap to the next line.
    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
    }
    // scrolling logic when cursor_row >= 25
    if (cursor_row >= 25) {
        // Move the text of every line up by one row.
        for (int i = 0; i < 24 * 80; i++) {
            VGA_BUFFER[i] = VGA_BUFFER[i + 80];
        }

        // Clear the last line.
        for (int i = 24 * 80; i < 25 * 80; i++) {
            VGA_BUFFER[i] = ' ' | 0x0F00;
        }
        
        // Set the cursor to the beginning of the last line.
        cursor_row = 24;
    }

    // Update the hardware cursor's position.
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
    // Loop through each of the 8 hex digits (nibbles)
    for (int i = 28; i >= 0; i -= 4) {
        // Get the 4 bits for this digit
        uint32_t nibble = (n >> i) & 0xF;
        // Convert the number (0-15) to its ASCII character ('0'-'9', 'A'-'F')
        char c = (nibble > 9) ? (nibble - 10 + 'A') : (nibble + '0');
        print_char(c);
    }
}