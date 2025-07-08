// myos/kernel/vga.h

#ifndef VGA_H
#define VGA_H

void clear_screen();
void update_cursor(int row, int col);
void print_string(const char* str);

#endif