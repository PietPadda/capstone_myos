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

    ; Set up our stack in a safe, low memory area BEFORE attempting the disk read.
    ; This prevents the loaded kernel data from overwriting the stack.
    mov sp, 0x5000 

    ; Enable the A20 line to access memory above 1MB.
    call enable_a20

    ; --- Load Kernel using modern LBA Extended Read ---
    ; Kernel is located starting at LBA 5 by our build script.
    mov si, dap                 ; Point SI to our Disk Address Packet
    ; Increase sectors to read to 2048 (1MB), ensuring our whole kernel
    ; and initial data structures are loaded into memory.
    mov word [si + 2], 2048       ; Increase sectors to read to X 512b sectors
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

    ; Move the stack to a much higher address (e.g., 0x900000) to avoid
    ; being overwritten by the 1MB kernel load.
    mov esp, 0x900000

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