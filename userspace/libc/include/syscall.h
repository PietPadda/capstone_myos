// myos/userspace/libc/include/syscall.h

#include <kernel/types.h>

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

// Wrapper for the "exit" syscall. It does not return.
static inline void syscall_exit() {
    // EAX=3 for our exit syscall
    __asm__ __volatile__ ("int $0x80" : : "a"(3));
}

// Wrapper for the "play_sound" syscall.
static inline void syscall_play_sound(uint32_t frequency) {
    // EAX=4 for our play_sound syscall
    // EBX=frequency
    __asm__ __volatile__ ("int $0x80" : : "a"(4), "b"(frequency));
}

// Wrapper for the "sleep" syscall.
static inline void syscall_sleep(uint32_t ms) {
    // EAX=5 for our sleep syscall
    // EBX=milliseconds
    __asm__ __volatile__ ("int $0x80" : : "a"(5), "b"(ms));
}

#endif