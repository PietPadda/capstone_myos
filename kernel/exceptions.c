// myos/kernel/exceptions.c

#include "exceptions.h"
#include "vga.h" // for printing to screen

// C-level handler for all exceptions
void fault_handler(registers_t r) {
    clear_screen(); // Start with a clean slate
    print_string("CPU Exception Caught!\n");
    print_string("Interrupt Number: ");
    print_hex(r.int_no); // Print the number from the registers struct
    print_string("\nSystem Halted.");

    // We hang forever.
    for (;;);
}