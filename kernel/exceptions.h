// myos/kernel/exceptions.h
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

void fault_handler(registers_t *r);

#endif