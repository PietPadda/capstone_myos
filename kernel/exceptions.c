// myos/kernel/exceptions.c

#include "exceptions.h"
#include "vga.h" // for printing to screen

// C-level handler for all exceptions
void fault_handler(registers_t r) {
    // A simple message indicating an exception has occurred.
    print_string("CPU Exception Caught! System Halted.");

    // We hang forever.
    for (;;);
}