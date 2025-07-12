// myos/kernel/shell.c

#include "shell.h"
#include "vga.h" // We need this for print_char
#include "string.h" // Needed for strcmp
#include "timer.h" // for tick getter
#include "io.h" // for port_byte_out
#include "memory.h" // for dynamic heap memory

// These labels are defined in data.asm
extern char file_start[];
extern char file_end[];

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

    // command arg parsing
    char* command = cmd_buffer;
    char* argument = 0; // Starts as null

    // Find the first space to separate command from argument
    int i = 0;
    while (command[i] != ' ' && command[i] != '\0') {
        i++;
    }

    if (command[i] == ' ') {
        // If we found a space, null-terminate the command part
        command[i] = '\0';
        // And the argument starts right after
        argument = command + i + 1;
    }

    // Command Handling
    print_string("\n");

    // help command
    if (strcmp(cmd_buffer, "help") == 0) {
        print_string("Available commands:\n  help - Display this message\n  cls  - Clear the screen\n  uptime  - Shows OS running time\n  reboot  - Reset the OS\n  memtest  - Print 3 dynamic heap addresses\n  cat  - Inspect file content in kernel memory\n");

    // cls command
    } else if (strcmp(cmd_buffer, "cls") == 0) {
        clear_screen();

    // uptime command
    } else if (strcmp(cmd_buffer, "uptime") == 0) {
        // Our timer is set to 100Hz, so 100 ticks = 1 second
        print_string("Uptime (seconds): ");
        print_hex(timer_get_ticks() / 100);

    // reboot command
    } else if (strcmp(cmd_buffer, "reboot") == 0) {
        print_string("Rebooting system...");
        // Send the reboot command to the keyboard controller
        port_byte_out(0x64, 0xFE);

    // memtest command
    } else if (strcmp(cmd_buffer, "memtest") == 0) {
        // Allocate a few blocks and print their addresses
        void* addr1 = malloc(1);
        void* addr2 = malloc(1);
        void* addr3 = malloc(1024);
        print_string("Block 1: "); print_hex((uint32_t)addr1);
        print_string("\nBlock 2: "); print_hex((uint32_t)addr2);
        print_string("\nBlock 3: "); print_hex((uint32_t)addr3);

    // cat command
    } else if (strcmp(cmd_buffer, "cat") == 0) {
        // For now, let's just prove we received the argument correctly.
        if (argument) {
            print_string("Argument received: '");
            print_string(argument);
            print_string("'");
        } else {
            // For now, if no argument, do what it did before
            print_string("Displaying embedded file:\n");
            extern char file_start[]; // Get the labels from data.asm
            extern char file_end[];
            for (char* p = file_start; p < file_end; p++) {
                print_char(*p);
            }
        }

    // invalid command
    } else if (cmd_index > 0) { // Only show error for non-empty commands
        print_string("Unknown command: ");
        print_string(cmd_buffer);
    }

    // Reset buffer for the next command
    cmd_index = 0;

    // Only print a newline for spacing if the command wasn't "cls".
    if (strcmp(cmd_buffer, "cls") != 0) {
        print_string("\n");
    }

    //print a new prompt for the next command
    print_string(PROMPT);
}