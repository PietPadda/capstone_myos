; myos/boot/bootloader.asm
; A bootloader that loads a kernel, enters 32-bit protected mode,
; and then jumps to the kernel.

org 0x7C00
bits 16

GDT_NULL equ 0
GDT_CODE equ 0x8
GDT_DATA equ 0x10

start:
    mov bh, dl      ; BIOS stores boot drive in DL, save it in BH

    ; Set up segments to a known state (0)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00  ; Set stack pointer below our code

    ; --- Reset disk system ---
    mov ah, 0x00
    mov dl, bh
    int 0x13
    jc disk_error

    ; --- Load Kernel from Disk into memory address 0x10000 ---
    mov ax, 0x1000      ; Set ES to 0x1000. ES:BX = 0x1000:0x0000 = 0x10000
    mov es, ax
    xor bx, bx          ; BX = 0

    mov ah, 0x02        ; BIOS read sectors function
    mov al, 10          ; Read 10 sectors (5 KB)
    mov ch, 0x00        ; Cylinder 0
    mov cl, 0x02        ; Sector 2 (Kernel starts after boot sector)
    mov dh, 0x00        ; Head 0
    mov dl, bh          ; Use the saved boot drive number
    int 0x13
    jc disk_error
    cmp al, 10          ; Verify number of sectors read
    jne disk_error

    ; --- Enter Protected Mode ---
    cli                 ; 1. Disable interrupts
    lgdt [cs:gdt_descriptor] ; 2. Load the GDT descriptor

    mov eax, cr0        ; 3. Set the PE bit in CR0
    or eax, 0x1
    mov cr0, eax

    ; 4. Far jump to our 32-bit code to flush the pipeline
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

; =======================================================
; ### 32-BIT PROTECTED MODE CODE ###
; =======================================================
bits 32
start_32bit:
    ; Set up 32-bit segment registers
    mov ax, GDT_DATA    ; 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up the stack
    mov esp, 0x90000

    ; Jump to the kernel's entry point
    jmp 0x10000

; =======================================================
; ### DATA SECTION ###
; =======================================================
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; GDT limit
    dd gdt_start               ; GDT base

gdt_start:
    ; Null descriptor
    dq 0x0

    ; Code segment descriptor (base=0, limit=4GB, 32-bit, Ring 0)
    dw 0xFFFF       ; Limit (low)
    dw 0x0          ; Base (low)
    db 0x0          ; Base (mid)
    db 10011010b    ; Access byte
    db 11001111b    ; Granularity and limit (high)
    db 0x0          ; Base (high)

    ; Data segment descriptor (base=0, limit=4GB, 32-bit, Ring 0)
    dw 0xFFFF       ; Limit (low)
    dw 0x0          ; Base (low)
    db 0x0          ; Base (mid)
    db 10010010b    ; Access byte
    db 11001111b    ; Granularity and limit (high)
    db 0x0          ; Base (high)
gdt_end:

msg_disk_error:
    db 'Disk read error!', 0

; =======================================================
; ### BOOT SIGNATURE ###
; =======================================================
times 510 - ($ - $$) db 0
dw 0xAA55