// myos/include/kernel/keyboard.h

#ifndef KEYBOARD_H
#define KEYBOARD_H

// Define special character codes for arrow keys
#define ARROW_UP    -1
#define ARROW_DOWN  -2
#define ARROW_LEFT  -3
#define ARROW_RIGHT -4

// Initializes the keyboard driver and registers its IRQ handler.
void keyboard_install();

// Reads a single character from the keyboard buffer, blocking if empty.
char keyboard_read_char();

// Clear the keyboard before running new cmds etc
void keyboard_flush();

#endif