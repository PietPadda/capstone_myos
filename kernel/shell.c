// myos/kernel/shell.c

#include "shell.h"
#include "vga.h" // We need this for print_char

void shell_init() {
    // We'll add code here in a later step.
}

// This function is called by the keyboard driver.
void shell_handle_input(char c) {
    // For now, just echo the character to the screen.
    print_char(c);
}