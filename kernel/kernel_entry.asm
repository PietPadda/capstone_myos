; myos/kernel/kernel_entry.asm
bits 32

; Tell the assembler that 'kmain' is in another file
extern kmain

; Make '_start' visible to the linker
global _start

_start:
    ; The bootloader has already set up the segments and stack.
    ; We can now safely call our main C function.
    call kmain

    ; If kmain returns (which it shouldn't), hang the system.
    cli
hang:
    hlt
    jmp hang