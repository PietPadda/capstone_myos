// myos/userspace/programs/user_test.c

void user_program_main() {
    // By creating the string here, its location is determined at runtime on the stack.
    // This makes the program self-contained and immune to the objcopy issue.
    const char* message = "--- SUCCESSFULLY CALLED SYSCALL ---";

    // The assembly is placed directly here to avoid any function call or linking issues.
    __asm__ __volatile__ (
        "int $0x80"
        : /* no output */
        : "a"(1), "b"(message) // Pass syscall number 1 in EAX and the message pointer in EBX
    );

    // Loop forever
    while(1) {}
}