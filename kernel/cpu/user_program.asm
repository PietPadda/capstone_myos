; myos/kernel/cpu/user_program.asm

bits 32
global user_program_start

user_program_start:
    ; Make our first system call!
    mov eax, 1      ; Syscall number 1 (our print function)
    int 0x80        ; Trigger the syscall interrupt

    ; Loop forever after the syscall
    jmp $