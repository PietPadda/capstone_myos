; myos/kernel/kernel_entry.asm

bits 32         ; We are in 32-bit protected mode.

extern kmain    ; Tell NASM that kmain is defined in another file.

; Define the entry point for the linker.
global _start
_start:
    ; The bootloader has already set up the stack pointer (ESP).
    ; We can now safely call our main C function.
    call kmain

    ; If kmain returns, hang the system. A real kernel's main
    ; loop should never exit.
    cli
    hlt