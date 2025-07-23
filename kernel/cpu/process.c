// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>
#include <kernel/fs.h>      // For filesystem functions
#include <kernel/memory.h>  // For malloc/free
#include <kernel/vga.h>     // For printing error messages
#include <kernel/elf.h>     // ELF loader struc
#include <kernel/string.h> // For memcpy and strlen

// We'll use a global to track the current user stack.
// In a multitasking OS, this would be part of a process structure.
void* current_user_stack = NULL;

// We need to access our TSS entry defined in tss.c
extern struct tss_entry_struct tss_entry;

// This is the function that performs the context switch to user mode.
// It sets up the stack for the IRET instruction and jumps.
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

// This function finds an ELF executable on disk, loads it into memory,
// and starts it as a new user-mode process
void exec_program(int argc, char* argv[]) {
    // The program to run is now passed as the first argument in the list.
    // e.g., in "run args.elf hello", argv[1] is "args.elf"
    const char* filename = argv[1]; 
    if (!filename) {
        print_string("run: Missing filename.");
        return;
    }

    // Find the file on disk using our filesystem driver.
    fat_dir_entry_t* file_entry = fs_find_file(filename);
    if (!file_entry) {
        print_string("run: File not found: ");
        print_string(filename);
        return;
    }

    // Read the entire file into a temporary buffer in kernel memory
    uint8_t* file_buffer = (uint8_t*)fs_read_file(file_entry);
    if (!file_buffer) {
        print_string("run: Not enough memory to load program.\n");
        return;
    }

    // Cast the beginning of the buffer to an ELF header
    Elf32_Ehdr* header = (Elf32_Ehdr*)file_buffer;

    // Parse the ELF header and validate the magic number
    if (header->magic != ELF_MAGIC) {
        print_string("run: Not an ELF executable.\n");
        free(file_buffer);
        return;
    }

    // Locate the program header table using the offset from the main heade
    Elf32_Phdr* phdrs = (Elf32_Phdr*)(file_buffer + header->phoff);

    // Iterate through program headers
    for (int i = 0; i < header->phnum; i++) {
        Elf32_Phdr* phdr = &phdrs[i];

        // We only care about "loadable" segments.
        if (phdr->type == PT_LOAD) {
            // Copy the segment from the file buffer to its final virtual address.
            uint8_t* dest = (uint8_t*)phdr->vaddr;
            uint8_t* src = file_buffer + phdr->offset;
            for (uint32_t j = 0; j < phdr->filesz; j++) {
                dest[j] = src[j];
            }

            // Handle the .bss section (uninitialized data)
            // memsz might be > filesz. The difference is the .bss section.
            if (phdr->memsz > phdr->filesz) {
                uint32_t bss_size = phdr->memsz - phdr->filesz;
                uint8_t* bss_start = dest + phdr->filesz;
                memset(bss_start, 0, bss_size);
            }
        }
    }

    // Allocate a separate, aligned stack for the user program.
    void* user_stack = malloc(USER_STACK_SIZE);
    if (!user_stack) {
        print_string("run: Not enough memory for user stack.\n");
        free(file_buffer);
        return;
    }
    // Track the stack so we can free it on exit.
    current_user_stack = user_stack;

    // Use uint32_t for stack pointer arithmetic
    uint32_t user_stack_top = (uint32_t)user_stack + USER_STACK_SIZE;

    // Place argc/argv onto the user stack
    // Copy argument strings to the top of the stack
    char* argv_strings_user[MAX_ARGS];
    for (int i = 0; i < argc; i++) {
        user_stack_top -= (strlen(argv[i]) + 1);
        argv_strings_user[i] = (char*)user_stack_top;
        memcpy(argv_strings_user[i], argv[i], strlen(argv[i]) + 1);
    }

    // Align stack to 4 bytes
    user_stack_top &= ~0x3;

    // Push the argv array (pointers to the strings)
    char** argv_user;
    user_stack_top -= sizeof(char*) * (argc + 1); // +1 for NULL terminator
    argv_user = (char**)user_stack_top;
    for (int i = 0; i < argc; i++) {
        argv_user[i] = argv_strings_user[i];
    }
    argv_user[argc] = NULL; // The list is NULL-terminated

    // Push argc and a pointer to argv (the arguments for main)
    user_stack_top -= sizeof(char**);
    *((char***)user_stack_top) = argv_user;
    user_stack_top -= sizeof(int);
    *((int*)user_stack_top) = argc;

    // The program is now in memory, so we can free the temporary file buffer
    free(file_buffer);

    // Jump to the program's entry point specified in the ELF header
    // Cast the final stack pointer back to void* for the function call
    switch_to_user_mode((void*)header->entry, user_stack_top);

    // This code is now truly unreachable, but for good practice,
    // if it were to be reached, we'd clean up.
    free(user_stack);
    current_user_stack = NULL;
}

