// myos/kernel/keyboard.c

#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "vga.h"

// Scancode set 1 for a US QWERTY keyboard layout.
// A 0 means the key is not printable.
unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// The main keyboard handler function, called by our IRQ dispatcher.
static void keyboard_handler(registers_t *r) {
    unsigned char scancode;

    // Read the scancode from the keyboard's data port.
    scancode = port_byte_in(0x60);

    // If the top bit of the byte is set, it means a key was released.
    // We only care about key presses, so we ignore releases for now.
    if (scancode & 0x80) {
        // Key release
    } else {
        // Key press
        print_char(kbd_us[scancode]);
    }
}

// The installation function that hooks the handler into the IRQ system.
void keyboard_install() {
    // Register `keyboard_handler` to be called when IRQ 1 is triggered.
    irq_install_handler(1, keyboard_handler);
}