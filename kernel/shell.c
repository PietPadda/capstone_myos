// myos/kernel/shell.c

#include "shell.h"
#include "vga.h" // We need this for print_char

#define PROMPT "PGOS> "
#define MAX_CMD_LEN 256

// A buffer to store the command being typed.
static char cmd_buffer[MAX_CMD_LEN];
// The current position in the command buffer.
static int cmd_index = 0;

// Initialize the shell.
void shell_init() {
    // Print the prompt when the shell starts.
    print_string(PROMPT);
}

// This function is called by the keyboard driver for each keypress.
void shell_handle_input(char c) {

    // If the user pressed Enter
    if (c == '\n') {
        print_char(c); // Echo the newline
        
        // For now, we don't process the command. Just reset the buffer.
        cmd_buffer[cmd_index] = '\0';
        cmd_index = 0;
        
        // Print the prompt for the next command
        print_string(PROMPT);
        
    // If the user pressed Backspace
    } else if (c == '\b') {
        if (cmd_index > 0) {
            // Go back one character in the buffer and on the screen.
            cmd_index--;
            print_char(c);
        }

    // For any other printable character
    } else if (cmd_index < MAX_CMD_LEN - 1) {
        // Add the character to the buffer and echo it to the screen.
        cmd_buffer[cmd_index++] = c;
        print_char(c);
    }
}