// myos/include/kernel/exceptions.h

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <kernel/types.h>

// This struct defines the registers pushed onto the stack.
// The order MUST EXACTLY MATCH the push order in isr.asm.
typedef struct {
    // Pushed by us manually, , so they are at the top of the frame.
    uint32_t gs, fs, es, ds;
    
    // Pushed by the 'pusha' instruction
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    // Pushed by our ISR stub (interrupt number and error code).
    uint32_t int_no, err_code;

    // Pushed by the processor automatically when an interrupt occurs.
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

void fault_handler(registers_t *r);

#endif