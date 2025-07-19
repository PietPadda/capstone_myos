// myos/userspace/programs/user_test.c

#include <syscall.h>

void _start() {
    syscall_print("Hello from user-mode C!\n");

    // Loop forever
    while(1) {}
}