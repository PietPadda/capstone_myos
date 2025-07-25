// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>
#include <kernel/fs.h>      // For filesystem functions
#include <kernel/memory.h>  // For malloc/free
#include <kernel/vga.h>     // For printing error messages
#include <kernel/elf.h>     // ELF loader struc
#include <kernel/string.h> // For memcpy and strlen
#include <kernel/debug.h>   // debug print

// The process table - now global
task_struct_t process_table[MAX_PROCESSES];

// Pointer to the currently running process
task_struct_t* current_task = NULL;

// global flag, initialized to 0 (false)
volatile int multitasking_enabled = 0;

// Let this file know about our new tasks
extern void idle_task();
extern void shell_task();

// We need to access our TSS entry defined in tss.c
extern struct tss_entry_struct tss_entry;

// This is the function that performs the context switch to user mode.
// It sets up the stack for the IRET instruction and jumps.
void switch_to_user_mode(void* entry_point, void* stack_ptr) {
    uint32_t current_cs, current_ds, current_ss, current_eflags;
    
    // Use inline assembly to get the current state of the CPU.
    __asm__ __volatile__ (
        "mov %%cs, %0;"
        "mov %%ds, %1;"
        "mov %%ss, %2;"
        "pushfl;"
        "popl %3;"
        : "=r"(current_cs), "=r"(current_ds), "=r"(current_ss), "=r"(current_eflags)
        :
        : "memory"
    );

    // Now we can print the *actual* values to the debug terminal.
    qemu_debug_string("IRET_DEBUG: C variables are: EIP=");
    qemu_debug_hex((uint32_t)entry_point);
    qemu_debug_string(", ESP=");
    qemu_debug_hex((uint32_t)stack_ptr);
    qemu_debug_string("\n");

    qemu_debug_string("IRET_DEBUG: Current kernel segment registers: CS=");
    qemu_debug_hex(current_cs);
    qemu_debug_string(", DS=");
    qemu_debug_hex(current_ds);
    qemu_debug_string(", SS=");
    qemu_debug_hex(current_ss);
    qemu_debug_string(", EFLAGS=");
    qemu_debug_hex(current_eflags);
    qemu_debug_string("\n");

    qemu_debug_string("IRET_DEBUG: About to switch to user mode (ring 3) with:\n");
    qemu_debug_string("IRET_DEBUG: SS=0x23, ESP=");
    qemu_debug_hex((uint32_t)stack_ptr);
    qemu_debug_string(", EFLAGS=0x202, CS=0x1B, EIP=");
    qemu_debug_hex((uint32_t)entry_point);
    qemu_debug_string("\n");
    
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
    qemu_debug_string("PROCESS: Entering exec_program.\n");
    qemu_debug_string("PROCESS: Filename is -> ");
    qemu_debug_string(argv[0]);
    qemu_debug_string("\n");
    // The shell has already processed the 'run' command.
    // argv[0] is now the filename of the program to execute.
    if (argc == 0) {
        print_string("run: Missing filename.\n");
         return;
    }
    const char* filename = argv[0];

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
    qemu_debug_string("PROCESS: ELF loaded successfully.\n");

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

    // Find a free slot in the process table
    task_struct_t* new_task = NULL;
    int new_pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TASK_STATE_UNUSED) {
            new_task = &process_table[i];
            new_pid = i;
            break;
        }
    }

    if (!new_task) {
        print_string("run: No free processes left.\n");
        free(file_buffer);
        return;
    }

    // Allocate a separate, aligned stack for the user program.
    void* user_stack = malloc(USER_STACK_SIZE);
    if (!user_stack) {
        print_string("run: Not enough memory for user stack.\n");
        free(file_buffer);
        return;
    }

    // Configure the new process's PCB
    new_task->pid = new_pid;
    new_task->state = TASK_STATE_RUNNING;
    strncpy(new_task->name, filename, PROCESS_NAME_LEN); // Use our new strncpy
    new_task->user_stack = user_stack; // Store stack in the PCB

    // Stack Setup
    // Work from the end of the stack buffer down to place arguments.
    uint32_t user_stack_top = (uint32_t)user_stack + USER_STACK_SIZE;

    // Copy argument strings to the top of the stack.
    // This creates a temporary buffer to hold the pointers to our strings on the user stack.
    char* temp_argv_pointers[MAX_ARGS];

    // Iterate backwards to place strings at lower addresses.
    for (int i = argc - 1; i >= 0; i--) {
        uint32_t len = strlen(argv[i]) + 1; // +1 for null terminator
        user_stack_top -= len;
        
        // Copy the string data from the kernel's stack to the user's stack.
        memcpy((void*)user_stack_top, argv[i], len);
        
        // Store the pointer to this new string in our temporary array.
        temp_argv_pointers[i] = (char*)user_stack_top;
    }

    // Align the stack pointer to a 4-byte boundary for the argv array.
    user_stack_top &= ~0x3;

    // Push the argv array (pointers to the strings) onto the user stack.
    user_stack_top -= sizeof(char*) * (argc + 1); // +1 for NULL terminator
    char** argv_on_stack = (char**)user_stack_top;

    // Copy the pointers from our temporary array to the new array on the user stack.
    for (int i = 0; i < argc; i++) {
        argv_on_stack[i] = temp_argv_pointers[i];
    }
    argv_on_stack[argc] = NULL; // Terminate the list with a NULL pointer

    // Push the argc and the pointer to the argv array itself.
    user_stack_top -= sizeof(char**);
    *((char***)user_stack_top) = argv_on_stack;
    user_stack_top -= sizeof(int);
    *((int*)user_stack_top) = argc;

    // The program is now in memory, so we can free the temporary file buffer
    free(file_buffer);

    // Set the new task as the currently running one BEFORE we make the switch.
    current_task = new_task;

    // Jump to the program's entry point specified in the ELF header
    // Cast the final stack pointer back to void* for the function call
    qemu_debug_string("PROCESS: Final user_stack_top: ");
    qemu_debug_hex(user_stack_top);
    qemu_debug_string("\n");

    qemu_debug_string("PROCESS: Program entry point: ");
    qemu_debug_hex(header->entry);
    qemu_debug_string("\n");
    
    // Check the values on the stack right before the switch
    // The top of the stack should now be argc.
    int* argc_ptr = (int*)user_stack_top;
    qemu_debug_string("PROCESS: Argc on stack: ");
    qemu_debug_hex(*argc_ptr);
    qemu_debug_string("\n");
    
    // The next value should be the argv pointer
    char*** argv_ptr_on_stack = (char***)(user_stack_top + sizeof(int));
    qemu_debug_string("PROCESS: Argv on stack: ");
    qemu_debug_hex((uint32_t)*argv_ptr_on_stack);
    qemu_debug_string("\n");

    // Jump to the program's entry point specified in the ELF header
    qemu_debug_string("PROCESS: Dumping user stack contents before switch...\n");
    // We'll dump the top 64 bytes, which should contain argc, argv,
    // and the pointers to the argument strings.
    qemu_debug_memdump((void*)user_stack_top, 64);

    qemu_debug_string("PROCESS: Switching to user mode at entry point ");
    qemu_debug_hex(header->entry);
    qemu_debug_string("\n");

    switch_to_user_mode((void*)header->entry, (void*)user_stack_top);
}

// Initializes the process table and creates the first kernel task.
void process_init() {
    // Clear the entire process table
    memset(process_table, 0, sizeof(process_table));

    // --- Task 0: The Idle Task ---
    // This task must always be present and runnable.
    void* stack_a = malloc(4096);
    process_table[0].pid = 0;
    process_table[0].state = TASK_STATE_RUNNING;
    strncpy(process_table[0].name, "idle", PROCESS_NAME_LEN);

    // sure that every task starts with a clean slate
    memset(&process_table[0].cpu_state, 0, sizeof(cpu_state_t));
    
    // Set up initial CPU state for IRET
    process_table[0].cpu_state.eip = (uint32_t)idle_task;
    process_table[0].cpu_state.cs = 0x08; // Kernel Code Segment
    process_table[0].cpu_state.ss = 0x10; // Kernel Data Segment
    process_table[0].cpu_state.eflags = 0x202; // Interrupts enabled
    // Set BOTH stack pointers for this kernel task
    process_table[0].cpu_state.esp = (uint32_t)stack_a + 4096;
    process_table[0].cpu_state.useresp = (uint32_t)stack_a + 4096;

    // --- Task 1: The Shell Task ---
    void* stack_b = malloc(4096);
    process_table[1].pid = 1;
    process_table[1].state = TASK_STATE_RUNNING;
    strncpy(process_table[1].name, "shell", PROCESS_NAME_LEN);

    // sure that every task starts with a clean slate
    memset(&process_table[1].cpu_state, 0, sizeof(cpu_state_t));
    
    // Set up initial CPU state for IRET
    process_table[1].cpu_state.eip = (uint32_t)shell_task;
    process_table[1].cpu_state.cs = 0x08;
    process_table[1].cpu_state.ss = 0x10;
    process_table[1].cpu_state.eflags = 0x202;
    
    // Set BOTH stack pointers for this kernel task
    process_table[1].cpu_state.esp = (uint32_t)stack_b + 4096;
    process_table[1].cpu_state.useresp = (uint32_t)stack_b + 4096;

    // Set the first task as the currently running one
    current_task = &process_table[0];

    /*// Initialize the first task (the kernel/shell itself)
    task_struct_t* shell_task = &process_table[0];
    shell_task->pid = 0;
    shell_task->state = TASK_STATE_RUNNING;
    strncpy(shell_task->name, "shell", PROCESS_NAME_LEN);
    shell_task->user_stack = NULL; // The shell runs in kernel mode, no user stack

    // Set the first task as the currently running one
    current_task = shell_task;*/
}

// This is our new, more intelligent round-robin scheduler.
cpu_state_t* schedule(registers_t *r) {
    qemu_debug_string("schedule: entered.\n");

    // This check is a safeguard against catastrophic failure.
    if (!current_task) {
        qemu_debug_string("schedule: FATAL - current_task is NULL. Halting.\n");
        for (;;) __asm__("cli; hlt");
    }
    qemu_debug_string("schedule: Saving state for PID ");
    qemu_debug_hex(current_task->pid);
    qemu_debug_string("...\n");

    // Save the CPU state of the current task
    // We only copy the registers that are part of cpu_state_t
    current_task->cpu_state.eax = r->eax;
    current_task->cpu_state.ecx = r->ecx;
    current_task->cpu_state.edx = r->edx;
    current_task->cpu_state.ebx = r->ebx;
    current_task->cpu_state.esp = r->esp;
    current_task->cpu_state.ebp = r->ebp;
    current_task->cpu_state.esi = r->esi;
    current_task->cpu_state.edi = r->edi;
    current_task->cpu_state.eip = r->eip;
    current_task->cpu_state.cs = r->cs;
    current_task->cpu_state.eflags = r->eflags;

    // For a same-privilege interrupt, the stack pointer to save is the
    // one that existed before PUSHA, which is stored in r->esp.
    current_task->cpu_state.useresp = r->esp;

    qemu_debug_string("schedule: State saved. Finding next task...\n");

    // Wake up sleeping tasks
    uint32_t now = timer_get_ticks();
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TASK_STATE_SLEEPING && now >= process_table[i].wakeup_time) {
            process_table[i].state = TASK_STATE_RUNNING;
            qemu_debug_string("schedule: Woke up PID ");
            qemu_debug_hex(i);
            qemu_debug_string(".\n");
        }
    }

    // Find the next runnable task
    int next_pid = current_task->pid;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        next_pid = (next_pid + 1) % MAX_PROCESSES;
        // We add "next_pid != 0" to temporarily skip the idle task
        // unless it's the only option left.
        if (process_table[next_pid].state == TASK_STATE_RUNNING && next_pid != 0) {
            // Found a runnable task, switch to it.
            current_task = &process_table[next_pid];
            qemu_debug_string("schedule: Switching to PID ");
            qemu_debug_hex(current_task->pid);
            qemu_debug_string(".\n");
            return &current_task->cpu_state;
        }
    }

    // Idle Case
     // If no other task was found, default to the idle task.
    current_task = &process_table[0];
    qemu_debug_string("schedule: No other task to switch to. Continuing with PID ");
    qemu_debug_hex(current_task->pid);
    qemu_debug_string(".\n");

    // Return a pointer to the NEW task's saved state
    // The assembly code will use this to load the new context.
    return &current_task->cpu_state;
}