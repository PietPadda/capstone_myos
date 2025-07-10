// myos/kernel/exceptions.c

#include "exceptions.h"
#include "vga.h" // for printing to screen

// C-level handler for all exceptions - now takes a pointer!
void fault_handler(registers_t *r) {
    clear_screen(); // Start with a clean slate
    print_string("CPU Exception Caught!\n\n");
    print_string("Interrupt Number: ");
    print_hex(r->int_no);
    print_string("\nError Code:       ");
    print_hex(r->err_code);
    print_string("\n\n-- Register Dump --\n");
    print_string("EIP: ");
    print_hex(r->eip);
    print_string("  CS: ");
    print_hex(r->cs);
    print_string("  EFLAGS: ");
    print_hex(r->eflags);
    print_string("\nESP: ");
    print_hex(r->esp);
    print_string("  EAX: ");
    print_hex(r->eax);
    print_string("\n\nSystem Halted.");

    // We hang forever.
    for (;;);
}