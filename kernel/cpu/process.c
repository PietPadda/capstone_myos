// myos/kernel/cpu/process.c

#include <kernel/cpu/process.h>
#include <kernel/cpu/tss.h>
#include <kernel/fs.h>      // For filesystem functions
#include <kernel/memory.h>  // For malloc/free
#include <kernel/vga.h>     // For printing error messages
#include <kernel/elf.h>     // ELF loader struc
#include <kernel/string.h> // For memcpy and strlen
#include <kernel/debug.h>   // debug print
#include <kernel/timer.h>

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

// Let this file know about the kernel's page directory
extern page_directory_t* kernel_directory;

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

#define USER_STACK_SIZE 4096         // 4KB stack for user programs
#define USER_STACK_TOP    0xE0000000 // User stacks will start at 3.5GB
#define USER_STACK_PAGES  4          // and be 16KB (4 pages) in size

// This function finds an ELF executable on disk, loads it into memory,
// and starts it as a new user-mode process in its own address space.
int exec_program(int argc, char* argv[]) {
    qemu_debug_string("PROCESS: Entering exec_program.\n");

    // The shell has already processed the 'run' command.
    // argv[0] is now the filename of the program to execute.

    // error handling
    if (argc == 0) {
        print_string("run: Missing filename.\n");
         return -1;
    }

    const char* filename = argv[0];
    qemu_debug_string("PROCESS: Filename is -> ");
    qemu_debug_string(filename);
    qemu_debug_string("\n");

    // Find the file on disk using our filesystem driver.
    fat_dir_entry_t* file_entry = fs_find_file(filename);

    // error handling
    if (!file_entry) {
        print_string("run: File not found: ");
        print_string(filename);
        return -1;
    }

    // Read the entire file into a temporary buffer in kernel memory
    uint8_t* file_buffer = (uint8_t*)fs_read_file(file_entry);
    if (!file_buffer) {
        print_string("run: Not enough memory to load program.\n");
        return -1;
    }

    // Cast the beginning of the buffer to an ELF header
    Elf32_Ehdr* header = (Elf32_Ehdr*)file_buffer;

    // Parse the ELF header and validate the magic number
    if (header->magic != ELF_MAGIC) {
        print_string("run: Not an ELF executable.\n");
        free(file_buffer);
        return -1;
    }
    qemu_debug_string("PROCESS: ELF loaded successfully.\n");

    // --- Address Space Creation ---
    // 1. Create a new, separate address space for the process.
    qemu_debug_string("PROCESS: Cloning kernel page directory...\n");
    page_directory_t* new_dir = paging_clone_directory(kernel_directory);

    // error handling
    if (!new_dir) {
        print_string("run: Could not create address space.\n");
        free(file_buffer);
        return -1;
    }
    qemu_debug_string("PROCESS: Page directory cloned.\n");
    
    // 2. Temporarily switch into the new address space to map the program.
    page_directory_t* old_dir = (page_directory_t*)current_task->cpu_state.cr3;
    qemu_debug_string("PROCESS: Temporarily switching to new address space to map pages...\n");
    paging_switch_directory(new_dir);

    // 3. Map the program's code and data segments from the ELF file.
    Elf32_Phdr* phdrs = (Elf32_Phdr*)(file_buffer + header->phoff);
    for (int i = 0; i < header->phnum; i++) {
        Elf32_Phdr* phdr = &phdrs[i];
        if (phdr->type == PT_LOAD) {
            // Map every page covered by this segment.
            for (uint32_t p = 0; p < phdr->memsz; p += PMM_FRAME_SIZE) {
                uint32_t virt_addr = phdr->vaddr + p;
                uint32_t phys_frame = (uint32_t)pmm_alloc_frame();

                // error handling
                if (!phys_frame) {
                    // Proper cleanup would be needed here in a production OS
                    paging_switch_directory(old_dir);
                    paging_free_directory(new_dir);
                    free(file_buffer);
                    print_string("run: Out of physical memory.\n");
                    return -1;
                }
                paging_map_page(new_dir, virt_addr, phys_frame, PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER);
            }
            // Now that the memory is mapped, copy the segment data from the file.
            memcpy((void*)phdr->vaddr, file_buffer + phdr->offset, phdr->filesz);

            // The .bss section needs to be zeroed. It's the part of the segment
            // from the end of the file data up to the full memory size.
            if (phdr->memsz > phdr->filesz) {
                uint32_t bss_start = phdr->vaddr + phdr->filesz;
                uint32_t bss_size = phdr->memsz - phdr->filesz;
                qemu_debug_string("PROCESS: Zeroing .bss section at ");
                qemu_debug_hex(bss_start);
                qemu_debug_string(" for ");
                qemu_debug_hex(bss_size);
                qemu_debug_string(" bytes.\n");
                memset((void*)bss_start, 0, bss_size);
            }
        }
    }

    // 4. Map the user stack at a high virtual address.
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        // Correctly calculate the virtual address for each stack page, moving downwards.
        // The first page (i=0) will contain USER_STACK_TOP itself.
        uint32_t virt_addr = (USER_STACK_TOP - USER_STACK_SIZE) + (i * PMM_FRAME_SIZE);
        uint32_t phys_frame = (uint32_t)pmm_alloc_frame();

        // error handling.
        if (!phys_frame) { 
            return -1; }
        paging_map_page(new_dir, virt_addr, phys_frame, PAGING_FLAG_PRESENT | PAGING_FLAG_RW | PAGING_FLAG_USER);
    }

    // Setup argc/argv on the new stack
    // We are still inside the new address space, so we can write to the stack's virtual address.
    uint32_t user_stack_top = USER_STACK_TOP;

    // A temporary array on the kernel stack to hold the new user-stack pointers to the argument strings
    char* user_argv[MAX_ARGS];

    // First, copy the string data for each argument onto the top of the user stack.
    // We iterate backwards to keep them in the correct order.
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        user_stack_top -= len;
        memcpy((void*)user_stack_top, argv[i], len);
        user_argv[i] = (char*)user_stack_top; // Store the new pointer
    }

    // Align the stack to a 4-byte boundary before pushing pointers
    user_stack_top &= ~0x3;

    // Now, push the pointers to the strings (the argv array) onto the stack.
    // We include one extra for the NULL terminator.
    user_stack_top -= sizeof(char*) * (argc + 1);
    char** argv_on_stack = (char**)user_stack_top;
    for (int i = 0; i < argc; i++) {
        argv_on_stack[i] = user_argv[i];
    }
    argv_on_stack[argc] = NULL; // The list must be NULL-terminated

    // Finally, push the main arguments that user_entry.asm expects:
    // the pointer to the argv array, and argc.
    
    // Push argv pointer
    user_stack_top -= sizeof(char**);
    *((uint32_t*)user_stack_top) = (uint32_t)argv_on_stack;

    // Push argc
    user_stack_top -= sizeof(int);
    *((int*)user_stack_top) = argc;
    
    // 5. Switch back to the original address space of the shell.
    qemu_debug_string("PROCESS: Page mapping complete. Switching back to original address space.\n");
    paging_switch_directory(old_dir);

    // Find a free process slot in the process table
    task_struct_t* new_task = NULL;

    // find new task
    int new_pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TASK_STATE_UNUSED) {
            new_task = &process_table[i];
            new_pid = i;
            break;
        }
    }

    // error handling
    if (!new_task) {
        print_string("run: No free processes left.\n");
        paging_free_directory(new_dir); // Clean up the created directory
        free(file_buffer);
        return -1; // Return -1 on failure
    }

    // Configure the new process's PCB
    new_task->pid = new_pid;
    new_task->state = TASK_STATE_RUNNING;
    strncpy(new_task->name, filename, PROCESS_NAME_LEN); // Use our new strncpy
    new_task->user_stack = (void*)USER_STACK_TOP;
    new_task->page_directory = new_dir; // Set the new address space

    // Set up the initial CPU state for the new process.
    memset(&new_task->cpu_state, 0, sizeof(cpu_state_t));
    new_task->cpu_state.eip = (uint32_t)header->entry;
    new_task->cpu_state.useresp = user_stack_top; // Use the final calculated stack top
    new_task->cpu_state.cs = 0x1B;  // User Code Segment
    new_task->cpu_state.ss = 0x23;  // User Data Segment
    new_task->cpu_state.eflags = 0x202; // Interrupts enabled
    new_task->cpu_state.cr3 = (uint32_t)new_task->page_directory; // Set physical address for CR3

    // The program is now in memory, so we can free the temporary file buffer
    qemu_debug_string("PROCESS: New task configured. Ready for scheduler.\n");
    qemu_debug_string("  PID: "); qemu_debug_hex(new_task->pid);
    qemu_debug_string("\n  EIP: "); qemu_debug_hex(new_task->cpu_state.eip);
    qemu_debug_string("\n  ESP: "); qemu_debug_hex(new_task->cpu_state.useresp);
    qemu_debug_string("\n  CR3: "); qemu_debug_hex(new_task->cpu_state.cr3);
    qemu_debug_string("\n");
    free(file_buffer);
    return new_pid; // Return the new PID to the caller (the shell)
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

    process_table[0].page_directory = kernel_directory; // All kernel tasks use the kernel's map

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

    process_table[0].cpu_state.cr3 = (uint32_t)kernel_directory; // Set the physical address for CR3

    // --- Task 1: The Shell Task ---
    void* stack_b = malloc(4096);
    process_table[1].pid = 1;
    process_table[1].state = TASK_STATE_RUNNING;
    strncpy(process_table[1].name, "shell", PROCESS_NAME_LEN);

    process_table[1].page_directory = kernel_directory; // All kernel tasks use the kernel's map

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

    process_table[1].cpu_state.cr3 = (uint32_t)kernel_directory; // Set the physical address for CR3

    // Set the first task as the currently running one
    current_task = &process_table[0];
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
    // current_task->cpu_state.esp = r->esp; // r->esp is the kernel stack pointer after pusha, not the task's ESP
    current_task->cpu_state.ebp = r->ebp;
    current_task->cpu_state.esi = r->esi;
    current_task->cpu_state.edi = r->edi;
    current_task->cpu_state.eip = r->eip;
    current_task->cpu_state.cs = r->cs;
    current_task->cpu_state.eflags = r->eflags;

    current_task->cpu_state.cr3 = (uint32_t)current_task->page_directory; // Save the physical address of the page directory

    // For a same-privilege interrupt, the stack pointer to save is the
    // one that existed before PUSHA, which is stored in r->esp.
    //current_task->cpu_state.useresp = r->esp;

    // Check if the interrupt came from user mode (ring 3).
    // The user code segment selector is 0x1B (index 3, RPL 3).
    // The kernel code segment selector is 0x08.
    if (r->cs == 0x1B) {
        // This was a user task. The CPU pushed useresp and ss.
        current_task->cpu_state.useresp = r->useresp;
        current_task->cpu_state.ss = r->ss;
    } else {
        // This was a kernel task. The CPU did not change stacks.
        // The task's stack pointer is the kernel stack pointer at the time of interrupt.
        current_task->cpu_state.useresp = r->esp;
        current_task->cpu_state.ss = 0x10; // Kernel Data Segment
    }

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
        if (process_table[next_pid].state == TASK_STATE_RUNNING) { // Simpler logic, idle task is just another task
            // Found a runnable task, switch to it.
            current_task = &process_table[next_pid];
            qemu_debug_string("schedule: Switching to PID ");
            qemu_debug_hex(current_task->pid);
            qemu_debug_string(".\n");
            return &current_task->cpu_state;
        }
    }

    // Idle Case
    // If no runnable task is found (should only happen if all are waiting/sleeping),
    // If no other task was found, default to the idle task.
    current_task = &process_table[0];
    qemu_debug_string("schedule: No other task to switch to. Continuing with PID ");
    qemu_debug_hex(current_task->pid);
    qemu_debug_string(".\n");

    // Return a pointer to the NEW task's saved state
    // The assembly code will use this to load the new context.
    return &current_task->cpu_state;
}