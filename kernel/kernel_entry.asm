; myos/kernel/kernel_entry.asm

bits 32         ; We are in 32-bit protected mode.

; These symbols are provided by the linker script
extern bss_start
extern bss_end

extern kmain    ; Tell NASM that kmain is defined in another file.

; Define the entry point for the linker.
global _start, idt_load ; Make idt_load global here too

_start:
    ; Crucial: Clear the direction flag.
    ; This ensures 'rep stosb' increments EDI, not decrements it.
    cld

    ; --- Clear the BSS section ---
    mov edi, bss_start      ; Destination address
    mov ecx, bss_end        ; Source address
    sub ecx, edi            ; The size of the BSS section
    xor eax, eax            ; Value to clear with (zero)
    rep stosb               ; Repeat store-byte ECX times


    ; The bootloader has already set up the stack pointer (ESP).
    ; We can now safely call our main C function.
    call kmain

    ; If kmain returns, hang the system. A real kernel's main
    ; loop should never exit.
    cli
    hlt

; This function is called from our C code to load the IDT pointer.
global idt_load
idt_load:
    ; The first argument (a pointer to our idt_ptr_struct) is on the stack.
    mov eax, [esp + 4]  ; Get the pointer from the stack
    lidt [eax]          ; Load the IDT pointer into the IDTR register
    ret                 ; Return to the C code