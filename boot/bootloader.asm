; myos/boot/bootloader.asm
org 0x7C00
bits 16

GDT_NULL equ 0
GDT_CODE equ 0x8
GDT_DATA equ 0x10

start:
    mov bh, dl
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x00
    mov dl, bh
    int 0x13
    jc disk_error

    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 10
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, bh
    int 0x13
    jc disk_error
    cmp al, 10
    jne disk_error

    cli
    lgdt [cs:gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp GDT_CODE:start_32bit

disk_error:
    mov si, msg_disk_error
print_error_loop:
    lodsb
    or al, al
    jz hang
    mov ah, 0x0E
    int 0x10
    jmp print_error_loop

hang:
    cli
    hlt
    jmp hang

bits 32
start_32bit:
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

gdt_start:
    dq 0x0
    dw 0xFFFF, 0x0, 0x9A, 0xCF, 0x0
    dw 0xFFFF, 0x0, 0x92, 0xCF, 0x0
gdt_end:

msg_disk_error:
    db 'Disk read error!', 0

times 510 - ($ - $$) db 0
dw 0xAA55