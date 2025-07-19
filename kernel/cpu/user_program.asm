; myos/kernel/cpu/user_program.asm

bits 32

section .data
    message db "Hello from user mode!", 0

section .text
global user_program_start

user_program_start:
    ; Make our first system call!
    mov eax, 1                  ; Syscall number for our print function
    mov ebx, message            ; Argument 1: pointer to the string
    int 0x80                    ; Trigger the syscall interrupt

    ; Loop forever after the syscall
    jmp $