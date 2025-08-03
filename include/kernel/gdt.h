// myos/include/kernel/gdt.h

#ifndef GDT_H
#define GDT_H

#include <kernel/types.h>

// GDT pointer structure
struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Tell the C compiler that our assembly function exists elsewhere.
extern void gdt_flush(struct gdt_ptr_struct* gdt_ptr);

// This function initializes the kernel's GDT.
void gdt_install();

// our public helper function
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif