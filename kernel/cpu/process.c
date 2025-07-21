// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>
#include <kernel/fs.h>      // For filesystem functions
#include <kernel/memory.h>  // For malloc/free
#include <kernel/vga.h>     // For printing error messages

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

    // This is the target physical address for the program, from the linker script.
    uint8_t* program_buffer = (uint8_t*)0x100000;

    // Read the file cluster by cluster directly into the target address.
    uint16_t current_cluster = file_entry->first_cluster_low;

    // Calculate the correct cluster size instead of hardcoding it.
    uint32_t bytes_per_cluster = bpb->sectors_per_cluster * bpb->bytes_per_sector;

    // Calculate the exact number of clusters to read
    uint32_t num_clusters = (file_entry->file_size + bytes_per_cluster - 1) / bytes_per_cluster;

    // Loop exactly that many times
    for (uint32_t i = 0; i < num_clusters; i++) {
        // Stop if the chain ends unexpectedly
        if (current_cluster >= 0xFF8) break; // 0xFF8 is End-of-Chain for FAT12

        fs_read_cluster(current_cluster, program_buffer);
        program_buffer += bytes_per_cluster;
        current_cluster = fs_get_fat_entry(current_cluster);
    }

    // Allocate a stack for the user program
    void* user_stack = malloc(USER_STACK_SIZE);
    if (!user_stack) {
        // No need to free program_buffer, as it's not on the heap.
        print_string("run: Not enough memory for user stack.\n");
        return;
    }

    // The stack grows downwards. The top must be aligned to a 4-byte boundary.
    void* user_stack_top = (void*)(((uint32_t)user_stack + USER_STACK_SIZE) & ~0x3);

    // Jump to the program's entry point, which is now correctly at 0x100000
    switch_to_user_mode((void*)0x100000, user_stack_top);

    // This part is unreachable, but we'll free the stack just in case.
    free(user_stack);
}

