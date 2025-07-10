// myos/kernel/exceptions.h

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"

// A struct which defines the registers we pushed onto the stack in isr.asm.
typedef struct {
   uint32_t ds;                                      // Pushed by us
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed by pusha
   uint32_t err_code, int_no;                        // Pushed by our ISR stub (err_code first!)
   uint32_t eip, cs, eflags, useresp, ss;            // Pushed by the processor automatically
} __attribute__((packed)) registers_t;               // No padding GCC attribute

void isr_install();
void fault_handler(registers_t *r); // Changed to take a pointer

#endif