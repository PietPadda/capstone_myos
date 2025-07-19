// myos/userspace/libc/include/syscall.h

#ifndef SYSCALL_H
#define SYSCALL_H

// A simple C wrapper for our "print" syscall.
// It uses inline assembly to load the registers and call int 0x80.
static inline void syscall_print(const char* message) {
    // EAX=1 for our print syscall
    // EBX=message pointer
    __asm__ __volatile__ ("int $0x80" : : "a"(1), "b"(message));
}

#endif