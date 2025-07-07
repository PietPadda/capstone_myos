// myos/kernel/exceptions.c

#include "exceptions.h"
#include "vga.h" // We will add a print function to this soon

// C-level handler for all exceptions
void fault_handler(registers_t r) {
    // For now, we'll just hang. In the future, we can print an error.
    for (;;);
}