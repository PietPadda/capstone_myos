// myos/userspace/programs/user_test.c

#include <syscall.h>

void user_program_main() {
    // The ELF loader now correctly loads the .rodata section,
    // so we can use a standard string literal.
    const char* message = "--- PRINT MSG FROM USER PROGRAM VIA SYSCALL LIBRARY  ---";

    // Call the kernel's print function via our syscall wrapper.
    syscall_print(message);

    // Loop forever
    while(1) {}
}