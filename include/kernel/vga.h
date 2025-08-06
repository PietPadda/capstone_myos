// myos/include/kernel/vga.h

#ifndef VGA_H
#define VGA_H

#include <kernel/types.h> // to include uint32_t

void clear_screen();
void update_cursor(int row, int col);
void print_string(const char* str);
void print_hex(uint32_t n);
void print_char(char c);
void print_hex(uint32_t n);
void print_dec(uint32_t n);
void print_char_color(char c, uint8_t color);
void print_bootscreen();

// public cursor API for the shell to use.
int vga_get_cursor_row();
int vga_get_cursor_col();
void vga_set_cursor_pos(int row, int col);

#endif