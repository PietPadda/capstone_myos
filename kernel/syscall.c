// myos/kernel/syscall.c

#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/exceptions.h> // For the full registers_t definition
#include <kernel/keyboard.h> // keyboard input
#include <kernel/timer.h>      // For sleep()
#include <kernel/io.h>         // For port_byte_out()

#define MAX_SYSCALLS 32

// The system call dispatch table
static syscall_t syscall_table[MAX_SYSCALLS];

// Our first syscall: takes a pointer to a string in EBX and prints it.
static void sys_test_print(registers_t *r) {
    char* message = (char*)r->ebx;
    print_string(message);
    print_char('\n');
}

// Syscall 2: Read a character from the keyboard, blocking until a key is pressed.
static void sys_getchar(registers_t *r) {
    char c = keyboard_read_char();
    // The return value of a syscall is placed in the EAX register.
    r->eax = c;
}

// Syscall 3: Exit the current program. For now, we'll just reboot.
static void sys_exit(registers_t *r) {
    print_string("\nProgram exited. Rebooting system...");
    sleep(1000); // Wait 1 second for the message to be visible
    port_byte_out(0x64, 0xFE);
}

void syscall_install() {
    // Install the test print syscall at index 1
    syscall_table[1] = &sys_test_print;
    // Install the new getchar syscall at index 2
    syscall_table[2] = &sys_getchar;
    // Install the new exit syscall at index 3
    syscall_table[3] = &sys_exit;
}

// The main C-level handler for all system calls
void syscall_handler(registers_t *r) {
    // Get the syscall number from the EAX register
    uint32_t syscall_num = r->eax;

    // Check if the number is valid (and not 0)
    if (syscall_num > 0 && syscall_num < MAX_SYSCALLS && syscall_table[syscall_num] != 0) {
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