// myos/kernel/drivers/vga.c

#include <kernel/vga.h>
#include <kernel/io.h>
#include <kernel/types.h>

// Define our screen dimensions as constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 50

// VGA register ports
#define VGA_MISC_WRITE 0x3C2
#define VGA_AC_INDEX 0x3C0
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_GC_INDEX 0x3CE
#define VGA_SC_INDEX 0x3C4
#define VGA_SC_DATA 0x3C5

// This function reprograms the VGA controller to enable 80x50 text mode.
// The sequence of register writes is a standard recipe for enabling the 400-scanline mode.
void vga_set_80x50_mode() {
    // We must unlock the CRTC registers before we can modify them.
    port_byte_out(VGA_CRTC_INDEX, 0x03);
    port_byte_out(VGA_CRTC_DATA, port_byte_in(VGA_CRTC_DATA) | 0x80);
    port_byte_out(VGA_CRTC_INDEX, 0x11);
    port_byte_out(VGA_CRTC_DATA, port_byte_in(VGA_CRTC_DATA) & 0x7F);

    // Write the new register values for 80x50 mode
    unsigned char regs[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    };
    uint16_t values[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x47, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x00, 0x96, 0xB9, 0xE3,
    };

    for (int i = 0; i < (VGA_HEIGHT - 1); i++) {
        port_byte_out(VGA_CRTC_INDEX, regs[i]);
        port_byte_out(VGA_CRTC_DATA, values[i]);
    }
}

// Define a static cursor position
static int cursor_row = 0;
static int cursor_col = 0;

// Define the VGA buffer as a global, constant pointer to a volatile memory region.
volatile unsigned short* const VGA_BUFFER = (unsigned short*)0xB8000;

// Clear the screen by filling it with spaces
void clear_screen() {
    int i;
    for (int i = 0; i < VGA_WIDTH  * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = (unsigned short)' ' | 0x0F00;
    }
    cursor_row = 0;
    cursor_col = 0;
    update_cursor(cursor_row, cursor_col);
}

// Updates the VGA cursor's position.
void update_cursor(int row, int col) {
    unsigned short position = (row * VGA_WIDTH) + col;

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
        VGA_BUFFER[(cursor_row * VGA_WIDTH) + cursor_col] = c | (color << 8);
        cursor_col++;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    update_cursor(cursor_row, cursor_col);
}

void print_char(char c) {
    // Disable interrupts to prevent a task switch during this function.
    __asm__ __volatile__("cli");

    // Handle backspace
    if (c == '\b') {
        // Only backspace if the cursor isn't at the very beginning.
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = (VGA_HEIGHT - 1); // Move to the end of the previous line.
        }
        // Write a blank space to the current cursor position to 'erase' the char.
        VGA_BUFFER[(cursor_row * VGA_WIDTH) + cursor_col] = ' ' | (0x0F << 8);

    // Handle newline
    } else if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        // Handle a normal character.
        // The new way: Directly write the character 'c' with a white-on-black
        // attribute (0x0F). This is more stable than the old method.
        VGA_BUFFER[(cursor_row * VGA_WIDTH) + cursor_col] = c | (0x0F << 8);
        cursor_col++;
    }

    // If we're at the end of the line, wrap to the next line.
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    // scrolling logic when cursor_row >= VGA_HEIGHT
    if (cursor_row >= VGA_HEIGHT) {
        // Move the text of every line up by one row.
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_HEIGHT; i++) {
            VGA_BUFFER[i] = VGA_BUFFER[i + VGA_WIDTH];
        }

        // Clear the last line.
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            VGA_BUFFER[i] = ' ' | (0x0F << 8);
        }
        
        // Set the cursor to the beginning of the last line.
        cursor_row = (VGA_HEIGHT - 1);
    }

    // Update the hardware cursor's position.
    update_cursor(cursor_row, cursor_col);

    // Re-enable interrupts now that we are done.
    __asm__ __volatile__("sti");
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