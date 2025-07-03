// myos/kernel/idt.h
#ifndef IDT_H
#define IDT_H

#include "types.h"

// Defines a single entry in the Interrupt Descriptor Table (a "gate")
struct idt_entry_struct {
   uint16_t base_low;    // The lower 16 bits of the handler function's address
   uint16_t selector;    // The GDT code segment selector to use
   uint8_t  always0;     // This must always be zero
   uint8_t  flags;       // The gate type and attributes
   uint16_t base_high;   // The upper 16 bits of the handler function's address
} __attribute__((packed));


// Defines the structure for the IDT pointer, which is loaded using the 'lidt' instruction
struct idt_ptr_struct {
   uint16_t limit;       // The size of the IDT in bytes minus 1
   uint32_t base;        // The linear address where the IDT starts
} __attribute__((packed));

#endif