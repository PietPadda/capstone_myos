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

// Wrapper for the "getchar" syscall.
static inline char syscall_getchar() {
    char result;
    // EAX=2 for our getchar syscall. The kernel's return value will be in EAX.
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(result)  // Output: store the final value of EAX into 'result'.
        : "a"(2)        // Input: set EAX to 2 before the interrupt.
    );
    return result;
}

#endif