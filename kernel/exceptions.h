// myos/kernel/exceptions.h

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"

// A struct which defines the registers we pushed onto the stack in isr.asm.
typedef struct {
   uint32_t ds;                                      // Data segment selector
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed by pusha
   uint32_t int_no, err_code;                        // Pushed by our ISR stub
   uint32_t eip, cs, eflags, useresp, ss;            // Pushed by the processor automatically
} registers_t;

void isr_install();

#endif