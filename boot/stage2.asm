; Stage 2 Bootloader
; This bootloader is loaded by Stage 1. It uses LBA to load the kernel,
; enters 32-bit protected mode, and then jumps to the kernel.

org 0x5000  ; We are loaded at 0x5000 by Stage 1
bits 16

GDT_CODE equ 0x08
GDT_DATA equ 0x10

start:
    ; Stage 1 passes the boot drive number in DL. We save it immediately.
    mov [boot_drive], dl

    ; Set up our own stack.
    mov sp, 0x7C00 ; Set stack pointer to a safe area below the original bootloader

    ; --- Load Kernel using modern LBA Extended Read ---
    ; Kernel is located starting at LBA 5 by our build script.
    mov si, dap                 ; Point SI to our Disk Address Packet
    mov word [si + 2], 64       ; Increase sectors to read to 64 (32KB)
    mov word [si + 4], 0x0000   ; Target buffer offset
    mov word [si + 6], 0x1000   ; Target buffer segment (ES:BX -> 0x1000:0000 = 0x10000)
    mov dword [si + 8], 5       ; LBA Start = 5 (after stage1 and stage2)

    mov ah, 0x42                ; BIOS Extended Read function
    mov dl, [boot_drive]        ; Use the saved drive number
    int 0x13
    jc disk_error

    ; --- Enter Protected Mode ---
    cli                         ; 1. Disable interrupts
    lgdt [gdt_descriptor]       ; 2. Load our GDT
    
    mov eax, cr0                ; 3. Set the PE bit in CR0 to enable protected mode
    or eax, 0x01
    mov cr0, eax

    ; CRITICAL: Reload all segment registers now that we are in protected mode.
    ; The far jump will only reload CS, but we need the others to be valid
    ; for a stable state.
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    jmp GDT_CODE:start_32bit    ; 4. Far jump to flush the CPU pipeline

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
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up the stack in a high memory area
    mov esp, 0x90000

    ; Jump to the kernel's entry point
    jmp 0x10000

; =======================================================
; ### DATA SECTION ###
; =======================================================
boot_drive:     db 0
msg_disk_error: db 'Stage 2 read error!', 0

; Disk Address Packet (DAP) for the extended read call
dap:
    db 0x10, 0
    dw 0        ; Number of sectors (filled at runtime)
    dd 0        ; Buffer address (filled at runtime)
    dq 0        ; LBA start (filled at runtime)

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; GDT limit
    dd gdt_start               ; GDT base

gdt_start:
    ; Null descriptor
    dq 0x0

    ; Code Segment Descriptor (base=0, limit=4GB, Ring 0)
    dw 0xFFFF    ; Limit (low)
    dw 0x0       ; Base (low)
    db 0x0       ; Base (mid)
    db 0x9A      ; Access (Present, Ring 0, Code, Exec/Read)
    db 0xCF      ; Granularity (4K pages, 32-bit)
    db 0x0       ; Base (high)

    ; Data Segment Descriptor (base=0, limit=4GB, Ring 0)
    dw 0xFFFF    ; Limit (low)
    dw 0x0       ; Base (low)
    db 0x0       ; Base (mid)
    db 0x92      ; Access (Present, Ring 0, Data, Read/Write)
    db 0xCF      ; Granularity (4K pages, 32-bit)
    db 0x0       ; Base (high)
gdt_end: