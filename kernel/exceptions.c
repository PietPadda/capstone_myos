// myos/kernel/exceptions.c

#include "exceptions.h"
#include "vga.h"

// Helper function to print the names of the set EFLAGS bits
static void print_eflags(uint32_t eflags) {
    print_string(" [ ");
    if (eflags & 0x1) print_string("CF ");
    if (eflags & 0x4) print_string("PF ");
    if (eflags & 0x10) print_string("AF ");
    if (eflags & 0x40) print_string("ZF ");
    if (eflags & 0x80) print_string("SF ");
    if (eflags & 0x200) print_string("IF ");
    if (eflags & 0x400) print_string("DF ");
    print_string("]");
}

// Helper function to dump a section of the stack
static void dump_stack(uint32_t *stack_ptr) {
    print_string("\n\n-- STACK DUMP --\n");
    for (int i = 0; i < 16; ++i) {
        uint32_t value = stack_ptr[i];
        print_hex((uint32_t)&stack_ptr[i]);
        print_string(": ");
        print_hex(value);
        print_string("\n");
    }
}

// C-level handler for all exceptions
void fault_handler(registers_t *r) {
    clear_screen();

    print_string("CPU EXCEPTION CAUGHT! -- SYSTEM HALTED\n\n");
    print_string("Interrupt Number: ");
    print_hex(r->int_no);
    print_string("\nError Code:       ");
    print_hex(r->err_code);
    print_string("\n\n-- REGISTER DUMP --\n");
    print_string("EAX: "); print_hex(r->eax); print_string("  EBX: "); print_hex(r->ebx);
    print_string("\nECX: "); print_hex(r->ecx); print_string("  EDX: "); print_hex(r->edx);
    print_string("\nESI: "); print_hex(r->esi); print_string("  EDI: "); print_hex(r->edi);
    print_string("\n\n-- SEGMENT DUMP --\n");
    print_string("CS:  "); print_hex(r->cs);  print_string("     DS:  "); print_hex(r->ds);
    print_string("\nSS:  "); print_hex(r->ss);
    print_string("\n\n-- CONTROL DUMP --\n");
    print_string("EIP: "); print_hex(r->eip); 
    print_string("\nEFLAGS: "); print_hex(r->eflags);
    print_eflags(r->eflags);

    // Dump the stack. We use r->useresp for the stack pointer at the time of the fault.
    dump_stack((uint32_t *)r->useresp);

    for (;;);
}