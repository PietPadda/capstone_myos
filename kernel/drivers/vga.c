// myos/kernel/drivers/vga.c

#include <kernel/vga.h>
#include <kernel/io.h>
#include <kernel/types.h>

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

// Prints a single character to the screen with a specific color.
// NOTE: This does not handle scrolling or special characters like print_char does.
// It is a simplified version for the boot screen.
void print_char_color(char c, uint8_t color) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = c | (color << 8);
        cursor_col++;
    }

    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
    }
    update_cursor(cursor_row, cursor_col);
}

void print_char(char c) {
    // Disable interrupts to prevent a task switch during this function.
    // __asm__ __volatile__("cli");

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
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = ' ' | (0x0F << 8);

    // Handle newline
    } else if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        // Handle a normal character.
        // The new way: Directly write the character 'c' with a white-on-black
        // attribute (0x0F). This is more stable than the old method.
        VGA_BUFFER[(cursor_row * 80) + cursor_col] = c | (0x0F << 8);
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
            VGA_BUFFER[i] = ' ' | (0x0F << 8);
        }
        
        // Set the cursor to the beginning of the last line.
        cursor_row = 24;
    }

    // Update the hardware cursor's position.
    update_cursor(cursor_row, cursor_col);

    // Re-enable interrupts now that we are done.
    // __asm__ __volatile__("sti");
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

// Prints a 32-bit unsigned integer in decimal format.
void print_dec(uint32_t n) {
    // base case
    if (n == 0) {
        print_char('0');
        return;
    }

    // recursion
    if (n >= 10) {
        print_dec(n / 10);
    }
    print_char((n % 10) + '0');
}

void print_bootscreen() {
    // Light Blue on Black
    uint8_t color = 0x09;

    const char* pgos_art =
        "\n"
        "    ____  __________  _____\n"
        "   / __ \/ ____/ __ \/ ___/\n"
        "  / /_/ / / __/ / / /\__ \ \n"
        " / ____/ /_/ / /_/ /___/ / \n"
        "/_/    \____/\____//____/  \n"
        "                           \n"
        "  Welcome to Pieters's OS! \n";

    int i = 0;
    while (pgos_art[i]) {
        print_char_color(pgos_art[i], color);
        i++;
    }
}