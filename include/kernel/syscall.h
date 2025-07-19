// myos/include/kernel/syscall.h

#ifndef SYSCALL_H
#define SYSCALL_H

#include <kernel/exceptions.h> // Include the header that defines registers_t

// Define a type for our system call handler functions
typedef void (*syscall_t)(registers_t *r);

void syscall_install();

#endif