// myos/include/kernel/shell.h

#ifndef SHELL_H
#define SHELL_H

// Initializes the shell.
void shell_init();

// Handles a single character of input.
void shell_handle_input(char c);

#endif