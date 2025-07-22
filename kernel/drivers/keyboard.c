// myos/kernel/drivers/keyboard.c

#include <kernel/keyboard.h>
#include <kernel/irq.h>
#include <kernel/io.h>
#include <kernel/shell.h>
#include <kernel/vga.h>

// New keyboard buffer
#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_buffer_read_idx = 0;
static volatile uint32_t kbd_buffer_write_idx = 0;

// --- State and Character Maps ---
static volatile int shift_pressed = 0;

// Scancode set 1 for a US QWERTY keyboard layout - Normal
unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     // 0-9
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', // 10-19
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   // 20-29
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  // 30-39 (0x1E='a')
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',  // 40-49
    'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0,           // 50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 60-69
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 110-119
    0, 0, 0, 0, 0, 0, 0, 0                              // 120-127
};

// Scancode set 1 for a US QWERTY keyboard layout - Shifted
unsigned char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',     // 0-9
    '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', // 10-19
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   // 20-29
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  // 30-39 (0x1E='A')
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',    // 40-49
    'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0,           // 50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 60-69
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // 110-119
    0, 0, 0, 0, 0, 0, 0, 0                              // 120-127
};

// The main keyboard handler function, called by our IRQ dispatcher.
static void keyboard_handler(registers_t *r) {
    unsigned char scancode = port_byte_in(0x60);

    // Handle a key release first
    if (scancode & 0x80) {
        // If a shift key was released, update the flag.
        if ((scancode & 0x7F) == 0x2A || (scancode & 0x7F) == 0x36) {
            shift_pressed = 0;
        }
        return; // Nothing more to do on key release.
    }

    // Handle a key press
    // If it's a shift key, update the flag and exit.
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    // If we're still here, it must be a normal character key.
    if (scancode < 128) {
        char character = shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
        
        if (character) {
            // Instead of calling the shell, add the character to our buffer
            if ((kbd_buffer_write_idx + 1) % KBD_BUFFER_SIZE != kbd_buffer_read_idx) {
                kbd_buffer[kbd_buffer_write_idx] = character;
                kbd_buffer_write_idx = (kbd_buffer_write_idx + 1) % KBD_BUFFER_SIZE;
            }
        }
    }
}

// The installation function that hooks the handler into the IRQ system.
void keyboard_install() {
    // Register `keyboard_handler` to be called when IRQ 1 is triggered.
    irq_install_handler(1, keyboard_handler);
}

// Blocks until a character is available and returns it.
char keyboard_read_char() {
    // Wait for a character to be available
    while (kbd_buffer_read_idx == kbd_buffer_write_idx) {
        //Atomically enable interrupts and then halt.
        // The CPU will wait here until the next interrupt (e.g., a keypress).
        __asm__ __volatile__("sti\n\thlt");
    }

    // Read the character from the buffer
    char c = kbd_buffer[kbd_buffer_read_idx];
    kbd_buffer_read_idx = (kbd_buffer_read_idx + 1) % KBD_BUFFER_SIZE;
    return c;
}