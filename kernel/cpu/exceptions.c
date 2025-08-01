// myos/kernel/cpu/exceptions.c

#include <kernel/exceptions.h>
#include <kernel/vga.h>

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
    print_string("\n-- STACK DUMP --\n");
    for (int i = 0; i < 13; ++i) {
        uint32_t value = stack_ptr[i];
        print_hex((uint32_t)&stack_ptr[i]);
        print_string(": ");
        print_hex(value);
        print_string("\n");
    }
}

// C-level handler for all exceptions
void fault_handler(registers_t *r) {
    // Read the CR2 register, which contains the linear address that caused the fault
    uint32_t faulting_address;
    __asm__ __volatile__("mov %%cr2, %0" : "=r" (faulting_address));

    // clear screen for fault handler
    clear_screen();

    print_string("CPU EXCEPTION CAUGHT! -- SYSTEM HALTED\n");
    print_string("Interrupt Number: "); print_hex(r->int_no);
    print_string("  Error Code: "); print_hex(r->err_code);
    print_string("\nFaulting Address: ");  print_hex(faulting_address); // Print CR2

    print_string("\n-- REGISTER DUMP --\n");
    print_string("EAX: "); print_hex(r->eax); print_string("  EBX: "); print_hex(r->ebx);
    print_string("  ECX: "); print_hex(r->ecx); print_string("\nEDX: "); print_hex(r->edx);
    print_string("  ESI: "); print_hex(r->esi); print_string("  EDI: "); print_hex(r->edi);

    print_string("\n-- SEGMENT DUMP --\n");
    print_string("CS:  "); print_hex(r->cs);  print_string("  DS:  "); print_hex(r->ds);
    print_string("  SS:  "); print_hex(r->ss);

    print_string("\n-- CONTROL DUMP --\n");
    print_string("EIP: "); print_hex(r->eip); 
    print_string("  EFLAGS: "); print_hex(r->eflags);
    print_eflags(r->eflags);

    // Dump the stack. We use r->useresp for the stack pointer at the time of the fault.
    dump_stack((uint32_t *)r->useresp);

    for (;;);
}