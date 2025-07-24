// myos/include/kernel/cpu/process.h

#ifndef PROCESS_H
#define PROCESS_H

#include <kernel/types.h>
#include <kernel/exceptions.h> // registers_t

#define MAX_ARGS 16 // Maximum number of command arguments
#define MAX_PROCESSES 16 // Maximum number of processes in the system
#define PROCESS_NAME_LEN 32 // Max length of a process name

// Enum for process states
typedef enum {
    TASK_STATE_UNUSED,    // This entry in the table is free
    TASK_STATE_RUNNING,   // The process is currently running or ready to run
    TASK_STATE_ZOMBIE     // The process has finished but is waiting to be cleaned up
} task_state_t;

// This struct contains ONLY the registers we need to save for a task switch.
typedef struct {
    // Pushed by 'pusha' - 8 registers * 4 bytes each = 32 bytes total
    uint32_t edi;     // Offset 0
    uint32_t esi;     // Offset 4
    uint32_t ebp;     // Offset 8
    uint32_t esp;     // Offset 12 (Kernel ESP at time of switch)
    uint32_t ebx;     // Offset 16
    uint32_t edx;     // Offset 20
    uint32_t ecx;     // Offset 24
    uint32_t eax;     // Offset 28

    // Pushed by CPU/IRET frame - 5 registers * 4 bytes each = 20 bytes total
    uint32_t eip;     // Offset 32
    uint32_t cs;      // Offset 36
    uint32_t eflags;  // Offset 40
    uint32_t useresp; // Offset 44 (The task's actual stack pointer)
    uint32_t ss;      // Offset 48
} __attribute__((packed)) cpu_state_t;

// The Process Control Block (PCB)
typedef struct {
    int pid;                            // Process ID
    task_state_t state;                 // The current state of the process
    char name[PROCESS_NAME_LEN];        // The process name
    void* user_stack;                   // Pointer to the user-mode stack
    cpu_state_t cpu_state;              //store the task's registers
    // We will add more fields here later (e.g., registers, memory maps)
} task_struct_t;

void switch_to_user_mode(void* entry_point, void* stack_ptr); // takes  entry point AND user stack pointer
void exec_program(int argc, char* argv[]);
void process_init();
void schedule(registers_t *r); // scheduler

#endif