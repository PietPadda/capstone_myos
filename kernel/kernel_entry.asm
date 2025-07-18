; myos/kernel/kernel_entry.asm
bits 32

; Tell the assembler that these symbols are defined in another file (the linker script)
extern kmain, bss_start, bss_end

; Make '_start' visible to the linker
global _start

_start:
    ; --- Clear the .bss section ---
    ; This is critical for C code to work correctly, as it ensures
    ; all static/global variables are initialized to zero.
    mov edi, bss_start      ; Put the starting address of .bss into EDI
    mov ecx, bss_end        ; Put the ending address of .bss into ECX
    sub ecx, edi            ; ECX now holds the size of .bss in bytes
    add ecx, 3              ; Add 3 to handle sizes that aren't a multiple of 4
    shr ecx, 2              ; Divide by 4 to get the number of 4-byte chunks (dwords)

    xor eax, eax            ; Set EAX to 0, the value we want to write
    cld                     ; Clear the direction flag, so `stosd` increments EDI
    rep stosd               ; Write EAX to [EDI] and repeat ECX times

    ; The bootloader has already set up the segments and stack.
    ; We can now safely call our main C function.
    call kmain

    ; If kmain returns (which it shouldn't), hang the system.
    cli
hang:
    hlt
    jmp hang