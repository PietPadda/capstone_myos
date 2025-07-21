// myos/include/kernel/cpu/process.h

#ifndef PROCESS_H
#define PROCESS_H

void switch_to_user_mode(void* entry_point, void* stack_ptr); // takes  entry point AND user stack pointer
void exec_program(const char* filename);

#endif