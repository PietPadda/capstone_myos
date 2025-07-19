// myos/kernel/syscall.c

#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/exceptions.h> // For registers_t

void syscall_install() {
    // In the future, we would set up the syscall table here.
    // For now, it's just a placeholder.
}

// The C-level handler for all system calls
void syscall_handler(registers_t *r) {
    // For now, we only have one system call: print a message
    // We'll use EAX to identify the syscall number
    if (r->eax == 1) {
        print_string("Syscall 1 called!\n");
    }
}