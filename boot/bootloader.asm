; myos/boot/bootloader.asm
; this is the absolute minimal bootloader.
; it will run in 16-bit real mode when loaded by the BIOS.

org 0x7C00              ; tells NASM that code will be loaded by BIOS at physical address 0x7C00.
bits 16                 ; instructs NASM  generate 16-bit instruction opcodes for Real Mode.

; --- GDT Constants (Labels for our Memory Map) ---
; Each "segment" (chunk of memory) in our map will have an "entry".
; Each entry is 8 bytes big. These numbers are just positions within the map.
GDT_NULL equ 0       ; Label for the very first (empty) entry in our map
GDT_CODE equ 0x8     ; Label for the entry describing where our program code is
GDT_DATA equ 0x10    ; Label for the entry describing where our program data is

start:                  ; label marks very first instruction bootloader will execute
    ; setup segment registers.
    xor ax, ax      ; Set AX to zero
    mov ds, ax      ; Set Data Segment to 0
    mov es, ax      ; Set Extra Segment to 0
    mov ss, ax      ; Set Stack Segment to 0

    ; set up the Stack Pointer (SP).
    ; stack grows downwards in x86. By starting SP at 0x7C00 (our segment base),
    ; stack will grow into memory *before* our code (addresses < 0x7C00),
    ; preventing it from overwriting the bootloader's own instructions (which are at 0x7C00 to 0x7DFF).
    mov sp, 0x7C00      ; Set SP to the beginning of our segment (offset 0x7C00 from its base, which is 0x7C0).

    ; --- Set video mode to 80x25 text mode ---
    mov ah, 0x00
    mov al, 0x03  ; Mode 3 is 80x25 16-color text mode.
    int 0x10      ; this is the command that sets the video mode!

    ; --- Print our welcome message to the screen ---
    mov si, msg_welcome ; Point SI to our message string.
    mov ah, 0x0E        ; Use BIOS teletype function.
print_loop:
    lodsb               ; Load byte from [DS:SI] into AL, advance SI.
    or al, al           ; Check if AL is zero (end of string).
    jz done_printing    ; If zero, we are done.
    int 0x10            ; Call BIOS video interrupt to print.
    jmp print_loop      ; Loop to the next character.

done_printing:
    ; --- Load Kernel from Disk ---
    mov ax, 0x1000      ; Set ES to 0x1000. ES:BX will be our destination address.
    mov es, ax
    xor bx, bx          ; Set BX to 0. Full address is ES:BX = 0x1000:0x0000 = 0x10000.

    mov ah, 0x02        ; BIOS read sectors function
    mov al, 32           ; Read X sectors (4KB), enough for a small kernel
    mov ch, 0x00        ; Cylinder 0
    mov cl, 0x02        ; Sector 2 (where our kernel starts)
    mov dh, 0x00        ; Head 0
    mov dl, 0x00        ; Drive 0 (A:)
    int 0x13            ; Call BIOS disk service

    jc disk_error       ; If the read failed, the Carry Flag is set. Jump to an error handler.

    cmp al, 32           ; Compare AL (sectors actually read) with X (sectors requested)
    jne disk_error      ; If they're Not Equal, jump to our error handler

    ; --- Enable A20 Line (Fast A20 Gate Method) ---
    in al, 0x92     ; Read from system control port A
    or al, 2        ; Set bit 1 (the A20 enable bit), turning it on
    out 0x92, al    ; Write the new value back to the port

    ; --- Disable interrupts (CLI) ---
    cli

    ; --- Load GDT (LGDT) ---
    ; This instruction loads the base address and limit of our GDT
    ; into the CPU's special 'GDTR' register. The CPU now knows
    ; where to find our memory map for Protected Mode.
    lgdt [gdt_descriptor]

    ; --- Set Protected Mode Enable (PE) bit in CR0 register ---
    ; This is the instruction that officially switches the CPU from
    ; 16-bit Real Mode to 32-bit Protected Mode.
    mov eax, cr0         ; Read the current value of CR0 into EAX (a 32-bit register)
    or eax, 0x1          ; Set the lowest bit (bit 0), which is the PE (Protected Mode Enable) bit
    mov cr0, eax         ; Write the modified value back to CR0

    ; --- Far Jump to 32-bit Protected Mode Code Segment ---
    ; This is a special jump that does two things:
    ; 1. Reloads the Code Segment (CS) register using our 32-bit GDT_CODE selector.
    ; 2. Jumps execution to the 'protected_mode_start' label.
    ; This clears any old 16-bit instructions from the CPU's internal "pipeline".
    jmp GDT_CODE:start_32bit

disk_error:
    mov ah, 0x0E    ; BIOS teletype function
    mov al, 'X'     ; Character to print
    int 0x10        ; Print it
    ; Fall through to hang

hang:
    cli             ; Disable interrupts
    hlt             ; Halt the CPU
    jmp hang        ; Loop just in case

; =======================================================
; Data Section
; All data used by the bootloader goes here.
; =======================================================

; --- GDT Pointer (for the CPU to find our GDT map) ---
; This special 6-byte structure tells the CPU:
; 1. The total size of our GDT map (minus 1).
; 2. The exact memory address where our GDT map starts.
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; The "size" of our GDT map (2 bytes)
    dd gdt_start               ; The "starting address" of our GDT map (4 bytes)

; --- Global Descriptor Table (GDT) - Our Memory Map ---
; This table defines how the CPU can access different chunks of memory.
gdt_start: ; This label marks the very beginning of our GDT map.
    ; 1. The "Null" Entry (Required Empty Slot) - always 8 bytes of zeros
    dq 0x0

    ; 2. Our "Code" Entry (for running program instructions) - 8 bytes
    ; This rule tells the CPU: "You can execute code from memory starting at address 0,
    ; all the way up to 4 Gigabytes. Treat this memory as 32-bit."
    dw 0xFFFF    ; Part of the "size" of this memory chunk
    dw 0x0       ; Part of the "starting address" of this memory chunk
    db 0x0       ; Another part of the "starting address"
    db 10011010b ; "Access" byte: describes how this memory can be used (e.g., executable)
    db 11001111b ; "Flags" and another part of the "size" (e.g., 32-bit, 4KB blocks)
    db 0x0       ; The last part of the "starting address"

    ; 3. Our "Data" Entry (for storing program data) - 8 bytes
    ; This rule tells the CPU: "You can read and write data from memory starting at address 0,
    ; all the way up to 4 Gigabytes. Treat this memory as 32-bit."
    dw 0xFFFF    ; Part of the "size" of this memory chunk
    dw 0x0       ; Part of the "starting address"
    db 0x0       ; Another part of the "starting address"
    db 10010010b ; "Access" byte: describes how this memory can be used (e.g., writable)
    db 11001111b ; "Flags" and another part of the "size" (e.g., 32-bit, 4KB blocks)
    db 0x0       ; The last part of the "starting address"
gdt_end: ; This label marks the end of our GDT map for now.

msg_welcome:
    db 'MyOS Bootloader: Loading Kernel...', 0

; IMPORTANT: The code below this far jump will be assembled as 32-bit.
; We need to tell NASM this!
bits 32 ; Instruct NASM to assemble all following code as 32-bit instructions.

; This label marks the very first instruction the CPU will execute in 32-bit Protected Mode.
start_32bit:
    ; We are now officially running in 32-bit Protected Mode!
    ; DEBUG: Confirm we reached 32-bit mode
    mov al, '1'
    out 0xE9, al

    ; --- Initialize 32-bit Segment Registers ---
    ; In Protected Mode, segment registers hold 'selectors' (offsets into the GDT),
    ; not direct base addresses. We load them with our GDT_DATA selector (0x10).
    mov ax, GDT_DATA     ; Load the GDT_DATA selector (0x10) into AX
    mov ds, ax           ; Set Data Segment register
    mov es, ax           ; Set Extra Segment register
    mov fs, ax           ; Set FS Segment register
    mov gs, ax           ; Set GS Segment register
    mov ss, ax           ; Set Stack Segment register

    ; --- Set up the 32-bit Stack Pointer (ESP) ---
    ; Choose a safe address for the 32-bit stack, e.g., near the end of 1MB (0x90000).
    ; The stack grows downwards in x86, so 0x90000 is a high address, safe from code.
    mov esp, 0x90000     ; Set the 32-bit Stack Pointer

    ; DEBUG: Confirm segment registers and stack are set up
    mov al, '2'
    out 0xE9, al

    ; Pass VGA buffer address to the kernel in EAX
    mov eax, 0xB8000

    ; Jump to kernel
    jmp 0x10000

; --------------------------------------------------------------------------
; Boot Signature (Crucial for BIOS)
; --------------------------------------------------------------------------
times 510 - ($ - $$) db 0 ; fill rest of 512-byte sector with zeros
                          ; ($ - $$) calculates current size from 'org 0x7C00'
                          ; ensures boot sector is precisely 512 bytes long before the signature
dw 0xAA55                 ; Boot signature: 0xAA55 must be at offset 510 (0x1FE) of boot sector
                          ; BIOS verifies signature to consider the sector "bootable"

