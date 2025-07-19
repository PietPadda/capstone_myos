// myos/kernel/syscall.c

#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/exceptions.h> // For the full registers_t definition

#define MAX_SYSCALLS 32

// The system call dispatch table
static syscall_t syscall_table[MAX_SYSCALLS];

// Our first syscall: a simple test print
static void sys_test_print(registers_t *r) {
    print_string("Syscall 1 called!\n");
}

void syscall_install() {
    // Install the test print syscall at index 1
    syscall_table[1] = &sys_test_print;
}

// The main C-level handler for all system calls
void syscall_handler(registers_t *r) {
    // Get the syscall number from the EAX register
    uint32_t syscall_num = r->eax;

    // Check if the number is valid
    if (syscall_num < MAX_SYSCALLS && syscall_table[syscall_num] != 0) {
        // If it is, look up the handler in our table
        syscall_t handler = syscall_table[syscall_num];
        // And call it
        handler(r);
    } else {
        // Invalid syscall number, print an error
        print_string("Invalid syscall number: ");
        print_hex(syscall_num);
        print_char('\n');
    }
}