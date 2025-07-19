// myos/userspace/programs/user_test.c

#include <syscall.h>

void user_program_main() {
    syscall_print("Hello from user-mode C!\n");

    // Loop forever
    while(1) {}
}