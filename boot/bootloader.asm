; myos/boot/bootloader.asm
; this is the absolute minimal bootloader.
; it will run in 16-bit real mode when loaded by the BIOS.

org 0x7C00              ; tells NASM that code will be loaded by BIOS at physical address 0x7C00.
bits 16                 ; instructs NASM  generate 16-bit instruction opcodes for Real Mode.

start:                  ; label marks very first instruction bootloader will execute
    ; setup segment registers.
    ; in 16-bit Real Mode, segment registers are explicitly set
    ; we set DS, ES, SS to current code segment (CS), typically holds 0x7C0.
    ; this makes all memory accesses relative to our code's base address (0x7C00).
    mov ax, cs          ; Move value from CS (Code Segment) into AX.
    mov ds, ax          ; Set DS (Data Segment) to same value as CS.
    mov es, ax          ; Set ES (Extra Segment) to same value as CS.
    mov ss, ax          ; Set SS (Stack Segment) to same value as CS.

    ; set up the Stack Pointer (SP).
    ; stack grows downwards in x86. By starting SP at 0x7C00 (our segment base),
    ; stack will grow into memory *before* our code (addresses < 0x7C00),
    ; preventing it from overwriting the bootloader's own instructions (which are at 0x7C00 to 0x7DFF).
    mov sp, 0x7C00      ; Set SP to the beginning of our segment (offset 0x7C00 from its base, which is 0x7C0).