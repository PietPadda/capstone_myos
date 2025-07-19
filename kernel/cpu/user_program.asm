; myos/kernel/cpu/user_program.asm

bits 32
global user_program_start

user_program_start:
    ; This is user mode! We'll just loop forever for now.
    jmp $