; Stage 1 Bootloader with embedded FAT12 BPB

org 0x7C00
bits 16

; --- FAT12 BIOS Parameter Block ---
; This structure is now part of our bootloader. fs.c will read this.
jmp short start_bootloader
nop

bpb_oem_name:           db 'MYOS    ' ; 8-byte OEM name
bpb_bytes_per_sector:   dw 512
bpb_sectors_per_cluster:db 1
bpb_reserved_sectors:   dw 1
bpb_num_fats:           db 2
bpb_root_dir_entries:   dw 224
bpb_total_sectors:      dw 2880  ; 2880 * 512 = 1.44MB
bpb_media_type:         db 0xF0  ; F0 for 1.44MB floppy
bpb_sectors_per_fat:    dw 9
bpb_sectors_per_track:  dw 18
bpb_num_heads:          dw 2
bpb_hidden_sectors:     dd 0
bpb_total_sectors_large:dd 0
drive_number:           db 0
_reserved:              db 1
ext_boot_signature:     db 0x29
volume_serial:          dd 0xdeadbeef
volume_label:           db 'MYOS BOOT '
fs_type:                db 'FAT12   '

; --- Bootloader Code ---
; The jmp at the top of the file brings us here.
start_bootloader:
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

; --- Boot Signature ---
times 510 - ($ - $$) db 0
dw 0xAA55