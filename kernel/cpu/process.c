// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>
#include <kernel/fs.h>      // For filesystem functions
#include <kernel/memory.h>  // For malloc/free
#include <kernel/vga.h>     // For printing error messages
#include <kernel/elf.h>     // ELF loader struc

// We need to access our TSS entry defined in tss.c
extern struct tss_entry_struct tss_entry;

void switch_to_user_mode(void* entry_point, void* stack_ptr) {
    __asm__ __volatile__ (
        "cli;"                  // Disable interrupts.

        // Manually move arguments into specific, safe registers immediately.
        // This prevents the compiler from reusing them for other tasks.
        "mov %0, %%ecx;"        // Move entry_point into ECX.
        "mov %1, %%edx;"        // Move stack_ptr into EDX.

        // Set the user-mode data segments using a *different* register (EAX).
        "mov $0x23, %%ax;"
        "mov %%ax, %%ds;"
        "mov %%ax, %%es;"
        "mov %%ax, %%fs;"
        "mov %%ax, %%gs;"

        // Prepare the IRET stack frame using our safe registers.
        "pushl $0x23;"          // SS: The user-mode stack segment.
        "pushl %%edx;"          // ESP: The user's new stack pointer (from our safe EDX).
        "pushfl;"               // EFLAGS: Push the kernel's EFLAGS register (32-bit).
        "popl %%eax;"           // Pop EFLAGS into EAX for modification.
        "orl $0x200, %%eax;"    // Set the IF flag (bit 9) to enable interrupts.
        "pushl %%eax;"          // Push the modified EFLAGS.
        "pushl $0x1B;"          // CS: The user-mode code segment.
        "pushl %%ecx;"          // EIP: The program's entry point (from our safe ECX).
        "iret;"                 // Execute the jump to user mode.

        : /* no output operands */
        // Input operands: Tell GCC to place the C variables into any general-purpose register.
        : "r"(entry_point), "r"(stack_ptr)
        // Clobber list: Tell GCC we modified EAX, ECX, and EDX.
        : "eax", "ecx", "edx"
    );
}

#define USER_STACK_SIZE 4096 // 4KB stack for user programs

void exec_program(const char* filename) {
    fat_dir_entry_t* file_entry = fs_find_file(filename);
    if (!file_entry) {
        print_string("run: File not found: ");
        print_string(filename);
        return;
    }

    // Read the entire file into a buffer
    uint8_t* file_buffer = (uint8_t*)fs_read_file(file_entry);
    if (!file_buffer) {
        print_string("run: Not enough memory to load program.\n");
        return;
    }

    // Cast the beginning of the buffer to an ELF header
    Elf32_Ehdr* header = (Elf32_Ehdr*)file_buffer;

    // Validate the ELF magic number
    if (header->magic != ELF_MAGIC) {
        print_string("run: Not an ELF executable.\n");
        free(file_buffer);
        return;
    }

    print_string("ELF file validated. Entry point: ");
    print_hex(header->entry);
    print_string("\n");

    // We will stop here for now. Free the buffer.
    free(file_buffer);
}

