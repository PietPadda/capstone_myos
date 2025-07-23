// myos/userspace/programs/args.c

#include <syscall.h>

// Tell the compiler that our assembly stub is defined elsewhere.
extern void _start_user_program();

// The function is no longer the entry point, but is called by the stub
void user_program_main(int argc, char* argv[]) {
    syscall_print("Program received arguments:\n");

    char buf[2] = {0}; // For printing numbers
    buf[0] = argc + '0';
    syscall_print("  argc: ");
    syscall_print(buf);
    syscall_print("\n");

    for (int i = 0; i < argc; i++) {
        syscall_print("  argv[");
        buf[0] = i + '0';
        syscall_print(buf);
        syscall_print("]: ");
        syscall_print(argv[i]);
        syscall_print("\n");
    }

    syscall_exit();
}