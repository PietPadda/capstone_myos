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
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t eip, cs, eflags, useresp, ss;          // Pushed by CPU on interrupt
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