// myos/include/kernel/shell.h

#ifndef SHELL_H
#define SHELL_H

#define PROMPT "PGOS> "
#define MAX_CMD_LEN 256
#define HISTORY_SIZE 10 // Store the last 10 commands

// global variables for the shell's state
extern char history_buffer[HISTORY_SIZE][MAX_CMD_LEN];
extern int history_count;
extern int history_index;
extern char current_line[MAX_CMD_LEN];
extern int cursor_pos;
extern int line_len;

// Initializes the shell.
void shell_init();

// Handles a single character of input.
void shell_handle_input(char c);

void shell_run();
void restart_shell();

#endif