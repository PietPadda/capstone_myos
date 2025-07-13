; myos/kernel/gdt_load.asm

bits 32

global gdt_flush ; Makes this function visible to C code

gdt_flush:
    ; Get the pointer to the GDT structure from the stack
    mov eax, [esp + 4]
    ; Load the new GDT into the CPU's GDTR register
    lgdt [eax]

    ; Do a far jump to our new code segment to flush the CPU pipeline
    ; and reload the segment registers.
    mov ax, 0x10  ; 0x10 is the offset of our new data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush ; 0x08 is the offset of our new code segment
.flush:
    ret