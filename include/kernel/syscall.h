// myos/include/kernel/syscall.h

#ifndef SYSCALL_H
#define SYSCALL_H

// Forward-declare registers_t to avoid circular dependencies
struct registers;

// Define a type for our system call handler functions
typedef void (*syscall_t)(struct registers *r);

void syscall_install();

#endif