// myos/kernel/shell.c

#include <kernel/shell.h>
#include <kernel/vga.h> // We need this for print_char
#include <kernel/string.h> // Needed for strcmp
#include <kernel/timer.h> // for tick getter
#include <kernel/io.h> // for port_byte_out
#include <kernel/memory.h> // for dynamic heap memory
#include <kernel/disk.h> // for disk sector read
#include <kernel/fs.h> // for FAT12 entries
#include <kernel/cpu/process.h> // User Mode Switch & MAX_ARGS
#include <kernel/keyboard.h> // We need this for keyboard_read_char
#include <kernel/debug.h> // debug printing

// Let the shell know about the process table defined in process.c
extern task_struct_t process_table[MAX_PROCESSES];

// Make the global current_task pointer visible to this file.
extern task_struct_t* current_task;

// global variables to hold the shell's state
char history_buffer[HISTORY_SIZE][MAX_CMD_LEN];
int history_count = 0;
int history_index = 0;
char current_line[MAX_CMD_LEN];
int cursor_pos = 0;
int line_len = 0;

// Forward-declaration for our command processor
void process_command();

// Initialize the shell.
void shell_init() {
    // Clear the current line and set cursor to the start.
    memset(current_line, 0, MAX_CMD_LEN);
    cursor_pos = 0;
    line_len = 0;

    // Reset history index to the end.
    history_index = history_count;

    print_string(PROMPT); // print our cmd prompt
}

// Handles a single character of input.
void shell_handle_input(char c) {
    if (c == ARROW_LEFT) {
        // only go left if not at the leftest point
        if (cursor_pos > 0) {
            cursor_pos--;
            shell_redraw_line();
        }
        // only go right if not at the max line length
    } else if (c == ARROW_RIGHT) {
        if (cursor_pos < line_len) {
            cursor_pos++;
            shell_redraw_line();
        }
    } else if (c == ARROW_UP) {
        // Navigate "up" or "back" in history
        if (history_index > 0) {
            history_index--;
            int actual_index = history_index % HISTORY_SIZE;
            strncpy(current_line, history_buffer[actual_index], MAX_CMD_LEN);
            // Move cursor to the end of the recalled line
            line_len = cursor_pos = strlen(current_line);
            shell_redraw_line();
        }
    } else if (c == ARROW_DOWN) {
        // Navigate "down" or "forward" in history
        if (history_index < history_count) {
            history_index++;
            if (history_index == history_count) {
                // We've reached the end, so clear the line for a new command
                current_line[0] = '\0';
            } else {
                // Get the next command from the buffer
                int actual_index = history_index % HISTORY_SIZE;
                strncpy(current_line, history_buffer[actual_index], MAX_CMD_LEN);
            }
            // Move cursor to the end of the line
            line_len = cursor_pos = strlen(current_line);
            shell_redraw_line();
        }
        // handle enter
    } else if (c == '\n') {
        print_char(c); // Echo the newline
        current_line[line_len] = '\0'; // Null-terminate the command.
        process_command();
        // handle backspace
    } else if (c == '\b') {
        if (cursor_pos > 0) {
            // Shift characters to the left to delete.
            for (int i = cursor_pos; i < line_len; i++) {
                current_line[i - 1] = current_line[i];
            }
            line_len--;
            current_line[line_len] = '\0'; // Null-terminate the new string.
            cursor_pos--;

            // Redraw the line to show the change.
            shell_redraw_line();
        }
        // Handle printable characters
    } else if (c >= ' ' && c <= '~') { 
        if (line_len < MAX_CMD_LEN - 1) {
            // Make space for the new character if not at end of line.
            for (int i = line_len; i > cursor_pos; i--) {
                current_line[i] = current_line[i - 1];
            }
            current_line[cursor_pos] = c;
            line_len++;
            cursor_pos++;
            
            // Redraw the line to show the new character.
            shell_redraw_line();
        }
    }
}

// The main loop for the shell.
void shell_run() {
    while (1) {
        // simple, direct call
        char c = keyboard_read_char();
        shell_handle_input(c);
    }
}

// cleanly restart the shell after a program exits.
void restart_shell() {
    // We are now the main thread of execution.
    // Re-initialize the shell and enter its main loop.
    shell_init(); 
    shell_run();  
}

// This function processes the completed command. 
void process_command() {
    // Trim leading whitespace from the raw command line
    char* start = current_line;
    while (*start == ' ') start++;

    // Save the pristine command to history FIRST
    // Don't save empty commands. 'start' points to the beginning of the command text.
    if (strlen(start) > 0) {
        // Copy the command into our history. Use strncpy for safety.
        strncpy(history_buffer[history_count % HISTORY_SIZE], start, MAX_CMD_LEN);
        history_count++;
        // Reset the history navigation index to point to the new, blank line.
        history_index = history_count;
    }

    // Now, proceed with destructive parsing
    // Parse the command line into argc and argv
    int argc = 0;
    char* argv[MAX_ARGS];
    // Zero out the argv array to prevent using garbage pointers
    // for arguments that don't exist.
    memset(argv, 0, sizeof(char*) * MAX_ARGS);

    char* word_start = start;
    while (*word_start && argc < MAX_ARGS) {
        // We have the start of a word. Find its end.
        char* word_end = word_start;
        while (*word_end && *word_end != ' ') {
            word_end++;
        }

        // Store the pointer to the start of the word.
        argv[argc++] = word_start;

        // Check if we are at the end of the entire command string.
        if (*word_end == ' ') {
            // We found a space, so this is not the last word.
            // Terminate the current word by overwriting the space.
            *word_end = '\0';
            
            // The next word starts after this space.
            word_start = word_end + 1;
            
            // Skip any additional spaces to find the true start.
            while (*word_start == ' ') {
                word_start++;
            }
        } else {
            // We hit the end of the line ('\0'), which means we just
            // processed the last argument. We can stop parsing now.
            break;
        }
    }

    // Handle empty or only-whitespace commands
    if (argc == 0) {
        print_string("\n");
        print_string(PROMPT);
        return;
    }

    // Command Handling
    print_string("\n");

    // help command
    if (strcmp(argv[0], "help") == 0) {
        print_string("Available commands:\n  help - Display this message\n  cls  - Clear the screen\n  uptime  - Shows OS running time\n  reboot  - Reset the OS\n  memtest  - Allocate, free then recycle memory\n  cat  - Reads .txt file contents (needs arg)\n  disktest  - Read LBA19 (root dir)\n  sleep  - Stops OS for X ticks\n  ls  - List files in root dir\n  dump  - Dump the first 128b of root dir buffer\n  run  - Run user mode program\n  ps  - Show process list\n  kill - Reap a zombie process by PID\n  beep - A4 note using PC Speaker\n\n  --- DEBUG COMMANDS ---\n  starttone - Starts a continuous tone\n  stoptone  - Stops the continuous tone\n\n");
    
    // cls command
    } else if (strcmp(argv[0], "cls") == 0) {
        clear_screen();

    // uptime command
    } else if (strcmp(argv[0], "uptime") == 0) {
        // Our timer is set to 100Hz, so 100 ticks = 1 second
        print_string("Uptime (seconds): ");
        print_hex(timer_get_ticks() / 100);

    // reboot command
    } else if (strcmp(argv[0], "reboot") == 0) {
        print_string("Rebooting system...");
        // Send the reboot command to the keyboard controller
        port_byte_out(0x64, 0xFE);

    // memtest command
    } else if (strcmp(argv[0], "memtest") == 0) {
        print_string("Allocating block A (16 bytes)...");
        void* block_a = malloc(16);
        print_string("\n  A is at: "); print_hex((uint32_t)block_a);

        print_string("\nAllocating block B (1024 bytes)...");
        void* block_b = malloc(1024);
        print_string("\n  B is at: "); print_hex((uint32_t)block_b);

        print_string("\nFreeing block A...");
        free(block_a);

        print_string("\nAllocating block C (8 bytes)...");
        void* block_c = malloc(8);
        print_string("\n  C is at: "); print_hex((uint32_t)block_c);
        print_string("\n(Note: C should have the same address as A)");

    // cat command
    } else if (strcmp(argv[0], "cat") == 0) {
        if (argc > 1) {
            fat_dir_entry_t* file_entry = fs_find_file(argv[1]);
            if (file_entry) {
                __asm__ __volatile__("cli"); // Disable interrupts
                uint8_t* buffer = (uint8_t*)fs_read_file(file_entry);
                if (buffer) {
                    for (uint32_t i = 0; i < file_entry->file_size; i++) {
                        print_char(buffer[i]);
                    }
                    // free the buffer to prev mem leaks
                    free(buffer);
                }
                __asm__ __volatile__("sti"); // Re-enable interrupts
            } else {
                print_string("File not found: ");
                print_string(argv[1]);
            }
        } else {
            print_string("Usage: cat <filename>");
        }

    // disktest command
    } else if (strcmp(argv[0], "disktest") == 0) {
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
        free(buffer); // Free the allocated buffer to prevent a leak

    // sleep command
    } else if (strcmp(argv[0], "sleep") == 0) {
        if (argc > 1) {
            int ms = atoi(argv[1]);
            print_string("Sleeping for ");
            print_string(argv[1]);
            print_string("ms...");
            sleep(ms);
        } else {
            print_string("Usage: sleep <milliseconds>");
        }

    // ls command
    } else if (strcmp(argv[0], "ls") == 0) {
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

    // dump command
    } else if (strcmp(argv[0], "dump") == 0) {
        extern uint8_t* root_directory_buffer;
        print_string("\nRoot Directory Buffer Dump:\n");
        for (int i = 0; i < 128; i++) { // Print the first 128 bytes
            print_hex(root_directory_buffer[i]);
            print_char(' ');
        }

    // run command
    } else if (strcmp(argv[0], "run") == 0) {
        if (argc > 1) {
            // Flush any lingering keyboard input before launching the program.
            keyboard_flush();
            int child_pid = exec_program(argc - 1, &argv[1]);
        
            if (child_pid >= 0) {
                // exec_program succeeded and returned with interrupts disabled.
                // We can now safely set our state to waiting.
                current_task->state = TASK_STATE_WAITING;

                // We yield the CPU. The IRET from the interrupt handler
                // will automatically re-enable interrupts for us.
                __asm__ __volatile__("int $0x20");
            } else {
                // exec_program failed, but it left interrupts disabled.
                // We must re-enable them before continuing the shell.
                __asm__ __volatile__("sti");
            }
        } else {
            print_string("Usage: run <filename>");
        }

    // ps command
    } else if (strcmp(argv[0], "ps") == 0) {
        print_string("PID  | State      | Name\n");
        print_string("----------------------------------\n");
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != TASK_STATE_UNUSED) {
                print_dec(process_table[i].pid);
                print_string("    | ");
                
                // Print the state as a string
                switch (process_table[i].state) {
                    case TASK_STATE_RUNNING:
                        print_string("Running    | ");
                        break;
                    case TASK_STATE_ZOMBIE:
                        print_string("Zombie     | ");
                        break;
                    default:
                        print_string("Unknown    | ");
                        break;
                }
                
                print_string(process_table[i].name);
                print_string("\n");
            }
        }

    // kill command
    } else if (strcmp(argv[0], "kill") == 0) {
        if (argc < 2) {
            print_string("Usage: kill <pid>");
        } else {
            int pid_to_kill = atoi(argv[1]);
            if (pid_to_kill > 0 && pid_to_kill < MAX_PROCESSES) {
                task_struct_t* task = &process_table[pid_to_kill];
                if (task->state == TASK_STATE_ZOMBIE) {
                    // The reaper (the shell) is now responsible for freeing the memory.
                    paging_free_directory(task->page_directory);
                    pmm_free_frame(task->kernel_stack);

                    // "Reap" the zombie by clearing its entire PCB entry.
                    memset(task, 0, sizeof(task_struct_t));
                    task->state = TASK_STATE_UNUSED;

                    print_string("Reaped zombie PID ");
                    print_dec(pid_to_kill);

                    // We log the frame count AFTER freeing to confirm it was restored.
                    qemu_debug_string("SHELL: Reaped zombie. Free frames: ");
                    qemu_debug_dec(pmm_get_free_frame_count());
                    qemu_debug_string("\n");
                } else {
                    print_string("Cannot kill non-zombie process.");
                }
            } else {
                print_string("Invalid PID.");
            }
        }

    // beep command
    } else if (strcmp(argv[0], "beep") == 0) {
        print_string("Beeping!");
        beep(440, 100); // Beep at X for Yms

    // start tone command
    } else if (strcmp(argv[0], "starttone") == 0) {
        print_string("Starting 440Hz tone...");
        play_sound(440);

    // stop tone command
    } else if (strcmp(argv[0], "stoptone") == 0) {
        print_string("Stopping tone...");
        nosound();

    // invalid command
    } else  { // Only show error for non-empty commands
        print_string("Unknown command: ");
        print_string(current_line);
        print_string("\n");
    }
    
    // Reset for the next command and print a new prompt.
    memset(current_line, 0, MAX_CMD_LEN);
    line_len = 0;
    cursor_pos = 0;

    // Only print a newline for spacing if the command wasn't "cls".
    if (strcmp(argv[0], "cls") != 0) {
        print_string("\n");
    }
    print_string(PROMPT);
}

// This function correctly redraws the shell's current line and cursor
// to account for the prompt and command buffer state.
void shell_redraw_line() {
    // Get the current physical cursor row from the VGA driver.
    int current_row = vga_get_cursor_row();

    // Define a pointer directly to the VGA text-mode buffer.
    volatile unsigned short* video_memory = (unsigned short*)0xB8000;
    // Define our standard text color (white on black).
    uint8_t color = 0x0F;

    // Erase the entire line by writing space characters directly to the buffer.
    //    This is a low-level operation with no side effects.
    for (int col = 0; col < 80; col++) {
        video_memory[(current_row * 80) + col] = ' ' | (color << 8);
    }

    // Write the prompt string directly to the buffer.
    int prompt_len = strlen(PROMPT);
    for (int i = 0; i < prompt_len; i++) {
        video_memory[(current_row * 80) + i] = PROMPT[i] | (color << 8);
    }
    
    // Write the current command buffer directly, right after the prompt.
    for (int i = 0; i < line_len; i++) {
        video_memory[(current_row * 80) + prompt_len + i] = current_line[i] | (color << 8);
    }

    // Finally, update the hardware cursor's position just once.
    vga_set_cursor_pos(current_row, prompt_len + cursor_pos);
}