// myos/include/kernel/keyboard.h

#ifndef KEYBOARD_H
#define KEYBOARD_H

// Initializes the keyboard driver and registers its IRQ handler.
void keyboard_install();

// Reads a single character from the keyboard buffer, blocking if empty.
char keyboard_read_char();

// Clear the keyboard before running new cmds etc
void keyboard_flush();

#endif