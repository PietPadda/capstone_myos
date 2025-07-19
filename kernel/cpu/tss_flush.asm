; myos/kernel/cpu/tss_flush.asm

bits 32
global tss_flush

tss_flush:
    ; The TSS selector is the 6th entry in the GDT, at offset 0x28 (5 * 8).
    ; The RPL bits (lowest 2) must be 0.
    mov ax, 0x28
    ltr ax
    ret