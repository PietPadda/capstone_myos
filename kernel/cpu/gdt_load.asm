; myos/kernel/cpu/gdt_load.asm

bits 32
global gdt_flush

; This function takes one argument: a pointer to the GDT descriptor structure.
gdt_flush:
    ; Get the pointer from the stack into EAX.
    mov eax, [esp+4]
    ; Load the GDT using the LGDT instruction.
    lgdt [eax]

    ; Reload all the data segment registers.
    ; 0x10 is the offset of our data segment in the GDT.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Perform a far jump to a label in this file.
    ; This is the only way to reload the CS (Code Segment) register.
    ; 0x08 is the offset of our code segment in the GDT.
    jmp 0x08:.flush

.flush:
    ; Return to the C code that called us.
    ret