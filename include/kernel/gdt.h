// myos/include/kernel/gdt.h

#ifndef GDT_H
#define GDT_H

#include <kernel/types.h>

// This function initializes the kernel's GDT.
void gdt_install();

// our public helper function
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif