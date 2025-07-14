; Stage 1 Bootloader
; This tiny bootloader is located in the boot sector (LBA 0). Its only job
; is to load Stage 2 from the disk and jump to it.

org 0x7C00
bits 16

start:
    ; The BIOS provides the boot drive in DL. We must save it immediately.
    mov [boot_drive], dl

    ; Set up segments and stack for a clean 16-bit environment
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; --- Load Stage 2 from Disk ---
    ; We will load 4 sectors starting from LBA 1 into memory at 0x5000.
    ; Stage 2 will be placed at LBA 1 by our build script.
    mov si, dap                 ; Point SI to our Disk Address Packet
    mov word [si + 4], 0x5000   ; Target buffer offset: 0x5000
    mov word [si + 6], 0x0000   ; Target buffer segment: 0x0000 (ES:BX -> 0x0000:5000 = 0x5000)
    mov dword [si + 8], 1       ; LBA Start = 1 (sector after this bootloader)

    mov ah, 0x42                ; BIOS Extended Read function
    mov dl, [boot_drive]        ; Use the saved drive number
    int 0x13
    jc disk_error

    ; --- Jump to Stage 2 ---
    ; Pass the boot drive number in DL for Stage 2 to use
    mov dl, [boot_drive]
    jmp 0x0000:0x5000

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

; --- DATA ---
boot_drive:     db 0
msg_disk_error: db 'Stage 1 read error!', 0

; Disk Address Packet (DAP) for the extended read call
dap:
    db 0x10, 0          ; Size of packet (16 bytes)
    dw 4                ; Sectors to read = 4
    dd 0                ; Buffer address (filled at runtime)
    dq 0                ; LBA start (filled at runtime)


times 510 - ($ - $$) db 0
dw 0xAA55