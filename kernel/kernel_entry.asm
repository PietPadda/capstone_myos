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

; This function is called from our C code to load the IDT pointer.
global idt_load
idt_load:
    ; The first argument (a pointer to our idt_ptr_struct) is on the stack.
    mov eax, [esp + 4]  ; Get the pointer from the stack
    lidt [eax]          ; Load the IDT pointer into the IDTR register
    ret                 ; Return to the C code