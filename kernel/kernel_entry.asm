; myos/kernel/kernel_entry.asm
bits 32
extern kmain
global _start
_start:
    call kmain
    cli
hang:
    hlt
    jmp hang