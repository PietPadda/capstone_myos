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

    ; Set the stack pointer to a known safe area (0x9000), well below
    ; where the kernel will be loaded (0x10000) and away from our own code (0x5000).
    mov sp, 0x9000

    ; Enable the A20 line to access memory above 1MB.
    call enable_a20

    ; --- Load Kernel using modern LBA Extended Read in a loop ---
    ; We will read 16 chunks of 32KB each to load a total of 512KB.
    ; 256KB = 512 sectors. 512 sectors / 64 sectors_per_chunk = 8 chunks.
    mov cx, 16                   ; CX will be our loop counter (16 chunks)
    mov dword [current_lba], 5  ; The kernel starts at LBA 5
    
    ; Set the initial memory destination segment to 0x1000 (for physical addr 0x10000)
    mov ax, 0x1000 ; Set initial destination segment to 0x1000
    mov es, ax

load_loop:
    ; Set up the Disk Address Packet for this chunk
    mov si, dap                 ; Point SI to our Disk Address Packet
    mov word [si + 2], 64       ; Read 64 sectors per loop (32KB) -- BIOS limit
    mov word [si + 4], 0x0000   ; Target buffer offset
    mov word [si + 6], es       ; Target buffer segment (Use the CURRENT segment in ES)
    mov ebx, [current_lba]
    mov [si + 8], ebx           ; LBA start for this chunk

    ; Call BIOS extended read
    mov ah, 0x42                ; BIOS Extended Read function
    mov dl, [boot_drive]        ; Use the saved drive number
    int 0x13
    jc disk_error               ; If any chunk fails, we abort

    ; Update state for the next chunk
    add ax, 0x800               ; Advance destination by 32KB (32768 bytes / 16 = 2048 paragraphs = 0x800)
    mov es, ax                  ; Update the segment register
    add dword [current_lba], 64 ; Advance the LBA start sector

    dec cx
    jnz load_loop

    ; --- Enter Protected Mode (AFTER loading loop is finished) ---
    cli                         ; Disable interrupts
    lgdt [gdt_descriptor]       ; Load our GDT
    
    mov eax, cr0                ; Set the PE bit in CR0 to enable protected mode
    or eax, 0x01
    mov cr0, eax

    ; CRITICAL: Reload all segment registers now that we are in protected mode.
    ; The far jump will only reload CS, but we need the others to be valid
    ; for a stable state.
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    jmp GDT_CODE:start_32bit    ; Far jump to flush the CPU pipeline

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

; --- A20 Gate Enable Routine (via Keyboard Controller) ---
enable_a20:
    cli
    call a20_wait_input  ; Wait for keyboard controller to be ready
    mov al, 0xAD         ; Command to disable keyboard
    out 0x64, al

    call a20_wait_input
    mov al, 0xD0         ; Command to read from controller's output port
    out 0x64, al

    call a20_wait_output ; Wait for data to be available
    in al, 0x60          ; Read the controller status byte
    push eax

    call a20_wait_input
    mov al, 0xD1         ; Command to write to controller's output port
    out 0x64, al

    call a20_wait_input
    pop eax
    or al, 2             ; Set bit 1 (the A20 bit)
    out 0x60, al         ; Write the modified status byte back

    call a20_wait_input
    mov al, 0xAE         ; Command to re-enable the keyboard
    out 0x64, al
    sti
    ret

; Helper: Wait for keyboard controller input buffer to be empty
a20_wait_input:
    in al, 0x64
    test al, 2
    jnz a20_wait_input
    ret

; Helper: Wait for keyboard controller output buffer to be full
a20_wait_output:
    in al, 0x64
    test al, 1
    jz a20_wait_output
    ret

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

    ; Move the stack to a safe address WITHIN our 4MB identity map.
    mov esp, 0x90000

    ; Jump to the kernel's entry point
    jmp 0x10000

; =======================================================
; ### DATA SECTION ###
; =======================================================
boot_drive:     db 0
msg_disk_error: db 'Stage 2 read error!', 0
current_lba:    dd 0    ; store the current LBA for the loop

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