// myos/userspace/programs/user_test.c

#include <syscall.h>

void user_program_main() {
    syscall_print("Press any key to echo it...");

    // Call the kernel to get a character (blocks until keypress)
    char c = syscall_getchar();

    // We need a small buffer to print the character
    char buffer[2] = {0};
    buffer[0] = c;

    // Call the kernel to print the character back
    syscall_print("You pressed: ");
    syscall_print(buffer);

    // Loop forever
    while(1) {}
}