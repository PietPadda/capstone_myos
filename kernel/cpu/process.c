// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>

// We need to access our TSS entry defined in tss.c
extern struct tss_entry_struct tss_entry;
// This is the starting address of our user program
extern void user_program_main();

// Kernel stack for our user process
static char kernel_stack[4096];

void switch_to_user_mode() {
    // 1. Set up the kernel stack for the TSS
    // The CPU will switch to this stack when an interrupt happens in user mode.
    tss_entry.esp0 = (uint32_t)kernel_stack + sizeof(kernel_stack);

    // 2. Prepare the iret stack frame
    __asm__ __volatile__ (
        "cli;"
        "mov $0x23, %ax;" // 0x23 is the user data segment selector (index 4, RPL 3)
        "mov %ax, %ds;"
        "mov %ax, %es;"
        "mov %ax, %fs;"
        "mov %ax, %gs;"
        "mov %esp, %eax;"
        "pushl $0x23;"      // SS (user data segment)
        "pushl %eax;"       // ESP (current stack pointer)
        "pushf;"            // EFLAGS
        "popl %eax;"        // Get EFLAGS
        "orl $0x200, %eax;" // Set the IF flag (bit 9), enabling interrupts in user mode
        "pushl %eax;"       // Push modified EFLAGS
        "pushl $0x1B;"      // CS (user code segment, index 3, RPL 3)
        "pushl $user_program_main;" // EIP C program's entry point
        "iret;"
    );
}