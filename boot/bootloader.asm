; myos/boot/bootloader.asm
; this is the absolute minimal bootloader.
; it will run in 16-bit real mode when loaded by the BIOS.

org 0x7C00              ; tells NASM that code will be loaded by BIOS at physical address 0x7C00.
bits 16                 ; instructs NASM  generate 16-bit instruction opcodes for Real Mode.

start:                  ; label marks  very first instruction bootloader will execute