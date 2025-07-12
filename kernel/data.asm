; myos/kernel/data.asm

bits 32

global file_start
global file_end

section .rodata

file_start:
    ; Include the raw binary contents of data.txt
    incbin "kernel/data.txt"
file_end: