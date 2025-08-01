// myos/include/kernel/debug.h

#ifndef DEBUG_H
#define DEBUG_H

#include <kernel/types.h>
#include <kernel/exceptions.h> // for registers_t
#include <kernel/cpu/process.h> // for cpu_state_t


// Sends a null-terminated string to the debug console
void qemu_debug_string(const char* str);
// Sends a hexadecimal value to the debug console
void qemu_debug_hex(uint32_t n);
// Dumps a block of memory to the debug console
void qemu_debug_memdump(const void* addr, size_t size);
// Dumps the state of the CPU registers
void qemu_debug_regs(registers_t *r);
// Dumps the state of a saved cpu_state_t struct
void qemu_debug_cpu_state(cpu_state_t *s);

#endif