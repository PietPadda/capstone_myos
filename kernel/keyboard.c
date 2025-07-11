// myos/kernel/keyboard.c

#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "vga.h"

// --- State and Character Maps ---
static int shift_pressed = 0;

// Scancode set 1 for a US QWERTY keyboard layout - Normal
// A 0 means the key is not printable.
unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// Scancode set 1 for a US QWERTY keyboard layout - Shifted
unsigned char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// The main keyboard handler function, called by our IRQ dispatcher.
static void keyboard_handler(registers_t *r) {
    unsigned char scancode;

    // Read the scancode from the keyboard's data port.
    scancode = port_byte_in(0x60);

    // --- Handle Key Releases ---
    if (scancode & 0x80) {
        // Remove the 0x80 bit to get the original press scancode
        scancode &= 0x7F; 
        if (scancode == 0x2A || scancode == 0x36) { // L/R Shift released
            shift_pressed = 0;
        }
        
    // --- Handle Key Presses ---
    } else {
        if (scancode == 0x2A || scancode == 0x36) { // L/R Shift pressed
            shift_pressed = 1;
        } else {
            // Select the correct character map based on shift state
            char character = shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
            
            // Only print if the character is valid (not 0)
            if (character) {
                print_char(character);
            }
        }
    }
}

// The installation function that hooks the handler into the IRQ system.
void keyboard_install() {
    // Register `keyboard_handler` to be called when IRQ 1 is triggered.
    irq_install_handler(1, keyboard_handler);
}