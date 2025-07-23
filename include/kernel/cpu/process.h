// myos/include/kernel/cpu/process.h

#ifndef PROCESS_H
#define PROCESS_H

#define MAX_ARGS 16 // Define the constant here

void switch_to_user_mode(void* entry_point, void* stack_ptr); // takes  entry point AND user stack pointer
void exec_program(int argc, char* argv[]);

#endif