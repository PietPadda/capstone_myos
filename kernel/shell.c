// myos/kernel/shell.c

#include "shell.h"
#include "vga.h" // We need this for print_char
#include "string.h" // Needed for strcmp
#include "timer.h" // for tick getter
#include "io.h" // for port_byte_out
#include "memory.h" // for dynamic heap memory
#include "disk.h" // for disk sector read
#include "fs.h" // for FAT12 entries

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
        print_string("Available commands:\n  help - Display this message\n  cls  - Clear the screen\n  uptime  - Shows OS running time\n  reboot  - Reset the OS\n  memtest  - Print 3 dynamic heap addresses\n  cat  - Check if filename can be found\n  disktest  - Read LBA19 (root dir)\n  sleep  - Stops OS for X ticks\n  ls  - List files in root dir\n  dump  - Dump the first 128b of root dir buffer\n");

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
    } else if (strcmp(command, "cat") == 0) {
        if (argument) {
            fat_dir_entry_t* file_entry = fs_find_file(argument);
            if (file_entry) {
                print_string("File found! Size: ");
                print_hex(file_entry->file_size);
            } else {
                print_string("File not found: ");
                print_string(argument);
            }
        } else {
            print_string("Usage: cat <filename>");
        }

    // disktest command
    } else if (strcmp(command, "disktest") == 0) {
        print_string("Reading LBA 19 (root dir)...\n");

        // Allocate a 512-byte buffer for the sector data
        uint8_t* buffer = (uint8_t*)malloc(512);

        // Read sector 19, which should be the start of the root directory
        read_disk_sector(19, buffer);

        // Print the first 32 bytes (the first directory entry)
        print_string("First 32 bytes: ");
        for (int i = 0; i < 32; i++) {
            print_hex(buffer[i]);
            print_char(' ');
        }
        // Note: We don't free the buffer because we haven't written free() yet!

    // sleep command
    } else if (strcmp(command, "sleep") == 0) {
        if (argument) {
            int ms = atoi(argument);
            print_string("Sleeping for ");
            print_string(argument);
            print_string("ms...");
            sleep(ms);
        } else {
            print_string("Usage: sleep <milliseconds>");
        }

    // ls command
    } else if (strcmp(command, "ls") == 0) {
        port_byte_out(0xE9, 'E'); // Entered 'ls' command handler
        // These labels are defined in fs.c
        extern uint8_t* root_directory_buffer;
        extern uint32_t root_directory_size;

        print_string("Name      Ext  Attr  Size\n");
        print_string("---------------------------\n");

        // Loop through every 32-byte entry in the root directory
        for (uint32_t i = 0; i < root_directory_size; i += 32) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(root_directory_buffer + i);

            // Stop if we've reached the end of the directory list
            if (entry->name[0] == 0x00) {
                break;
            }
            // Skip deleted files
            if (entry->name[0] == 0xE5) {
                continue;
            }

            // Skip Long Filename entries which have a special attribute
            if ((entry->attributes & 0x0F) == 0x0F) {
                continue;
            }
            port_byte_out(0xE9, 'F'); // Found a valid file entry to list

            // Skip the Volume Label entry
            if (entry->attributes & 0x08) {
                continue;
            }

            // Print the 8.3 filename
            for (int j = 0; j < 8; j++) print_char(entry->name[j]);
            print_string("  ");
            for (int j = 0; j < 3; j++) print_char(entry->extension[j]);

            // Print attributes and size
            print_string("  0x"); print_hex(entry->attributes);
            print_string("  ");  print_hex(entry->file_size);
            print_string("\n");
        }
        port_byte_out(0xE9, 'G'); // Finished 'ls' command handler

    // dump command
    } else if (strcmp(command, "dump") == 0) {
        extern uint8_t* root_directory_buffer;
        print_string("\nRoot Directory Buffer Dump:\n");
        for (int i = 0; i < 128; i++) { // Print the first 128 bytes
            print_hex(root_directory_buffer[i]);
            print_char(' ');
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