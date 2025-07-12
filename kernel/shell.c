// myos/kernel/shell.c

#include "shell.h"
#include "vga.h" // We need this for print_char
#include "string.h" // Needed for strcmp
#include "timer.h" // for tick getter

#define PROMPT "PGOS> "
#define MAX_CMD_LEN 256

// Static variables to hold the command buffer state
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

// Forward-declaration for our command processor
void process_command();

// Initialize the shell.
void shell_init() {
    // first prompt
    print_string(PROMPT);
}

// This function is called by the keyboard driver for each keypress.
void shell_handle_input(char c) {
    // On Enter, null-terminate the string and process it
    if (c == '\n') {
        print_char(c); // Echo the newline

        cmd_buffer[cmd_index] = '\0';
        process_command();

        
    // On Backspace, delete a character
    } else if (c == '\b') {
        if (cmd_index > 0) {
            // Go back one character in the buffer and on the screen.
            cmd_index--;
            print_char(c);
        }

    // On any other key, add it to the buffer
    } else if (cmd_index < MAX_CMD_LEN - 1) {
        // Add the character to the buffer and echo it to the screen.
        cmd_buffer[cmd_index++] = c;
        print_char(c);
    }
}

// This function processes the completed command
void process_command() {
    // help command
    if (strcmp(cmd_buffer, "help") == 0) {
        print_string("Available commands:\n  help - Display this message\n  cls  - Clear the screen\n  uptime  - Shows OS running time\n");

    // cls command
    } else if (strcmp(cmd_buffer, "cls") == 0) {
        clear_screen();

    // uptime command
    } else if (strcmp(cmd_buffer, "uptime") == 0) {
        // Our timer is set to 100Hz, so 100 ticks = 1 second
        print_string("Uptime (seconds): ");
        print_hex(timer_get_ticks() / 100);

    // invalid command
    } else if (cmd_index > 0) { // Only show error for non-empty commands
        print_string("Unknown command: ");
        print_string(cmd_buffer);
    }

    // Only print a newline for spacing if the command wasn't "cls".
    if (strcmp(cmd_buffer, "cls") != 0) {
        print_string("\n");
    }

    // Reset buffer and print a new prompt for the next command
    cmd_index = 0;
    print_string(PROMPT);
}