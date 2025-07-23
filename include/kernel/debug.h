// myos/include/kernel/debug.h

#ifndef DEBUG_H
#define DEBUG_H

#include <kernel/types.h>

// Sends a null-terminated string to the QEMU debug console
void qemu_debug_string(const char* str);
// Sends a hexadecimal value to the QEMU debug console
void qemu_debug_hex(uint32_t n);
// Dumps a block of memory to the QEMU debug console
void qemu_debug_memdump(const void* addr, size_t size);

#endif