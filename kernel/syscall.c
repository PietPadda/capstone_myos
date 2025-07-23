// myos/kernel/syscall.c

#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/exceptions.h>  // registers_t definition
#include <kernel/keyboard.h>    // keyboard input
#include <kernel/timer.h>       // sleep()
#include <kernel/io.h>          // port_byte_out()
#include <kernel/shell.h>       // restart_shell()
#include <kernel/cpu/tss.h>     // tss_entry
#include <kernel/memory.h>      // free()
#include <kernel/debug.h>       // debug print

#define MAX_SYSCALLS 32

// We need access to the TSS to get the kernel stack pointer.
extern struct tss_entry_struct tss_entry;

// A temporary global to track the stack we need to free.
extern void* current_user_stack;

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

// Syscall 3: Exit the current program and return to the shell.
static void sys_exit(registers_t *r) {
    qemu_debug_string("SYSCALL: Entering sys_exit (syscall 3).\n");
    // Clean up resources (the user stack).
    if (current_user_stack) {
        qemu_debug_string("SYSCALL: Freeing user stack.\n");
        free(current_user_stack);
        current_user_stack = NULL;
    }

    qemu_debug_string("SYSCALL: Preparing return to shell...\n");
    // Prepare to return to the kernel shell instead of the user program.
    // We do this by modifying the stack frame that the `iret` instruction will use.
    r->eip = (uint32_t)restart_shell;
    r->cs = 0x08;      // Kernel Code Segment
    r->ss = 0x10;      // Kernel Stack Segment
    r->useresp = tss_entry.esp0;
    r->eflags = 0x202; // Interrupts enabled (IF = 1), plus a mandatory bit.
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