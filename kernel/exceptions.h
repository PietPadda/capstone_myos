// myos/kernel/exceptions.h

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"

// A struct which defines the registers we pushed onto the stack in isr.asm.
// This order MUST match the order of the push instructions in isr.asm.
typedef struct {
    // Pushed by us manually
    uint32_t gs, fs, es, ds;
    
    // Pushed by the 'pusha' instruction
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    // Pushed by our ISR stub (int_no first, then error code)
    uint32_t int_no, err_code;

    // Pushed by the processor automatically on interrupt
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

void isr_install();
void fault_handler(registers_t *r);

#endif