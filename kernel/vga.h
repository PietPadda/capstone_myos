// myos/kernel/vga.h

#ifndef VGA_H
#define VGA_H

#include "types.h" // to include uint32_t

void clear_screen();
void update_cursor(int row, int col);
void print_string(const char* str);
void print_hex(uint32_t n);
void print_char(char c);
void print_hex(uint32_t n);

#endif