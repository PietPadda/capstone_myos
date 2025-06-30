; myos/boot/bootloader.asm
; this is the absolute minimal bootloader.
; it will run in 16-bit real mode when loaded by the BIOS.

org 0x7C00              ; tells NASM that code will be loaded by BIOS at physical address 0x7C00.
bits 16                 ; instructs NASM  generate 16-bit instruction opcodes for Real Mode.

start:                  ; label marks very first instruction bootloader will execute
    ; setup segment registers.
    ; in 16-bit Real Mode, segment registers are explicitly set
    ; we set DS, ES, SS to current code segment (CS), typically holds 0x7C0.
    ; this makes all memory accesses relative to our code's base address (0x7C00).
    mov ax, cs          ; Move value from CS (Code Segment) into AX.
    mov ds, ax          ; Set DS (Data Segment) to same value as CS.
    mov es, ax          ; Set ES (Extra Segment) to same value as CS.
    mov ss, ax          ; Set SS (Stack Segment) to same value as CS.

    ; set up the Stack Pointer (SP).
    ; stack grows downwards in x86. By starting SP at 0x7C00 (our segment base),
    ; stack will grow into memory *before* our code (addresses < 0x7C00),
    ; preventing it from overwriting the bootloader's own instructions (which are at 0x7C00 to 0x7DFF).
    mov sp, 0x7C00      ; Set SP to the beginning of our segment (offset 0x7C00 from its base, which is 0x7C0).

    ; --- Set video mode to 80x25 text mode ---
    mov ah, 0x00
    mov al, 0x03  ; Mode 3 is 80x25 16-color text mode.
    int 0x10

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
    ; --- Halt the CPU ---
    cli                 ; Disable interrupts.
hang:
    hlt                 ; Halt the CPU.
    jmp hang            ; Loop here to be safe.


; --- Data Section ---
msg_welcome:
    db 'MyOS has booted successfully!', 0

; --------------------------------------------------------------------------
; Boot Signature (Crucial for BIOS)
; --------------------------------------------------------------------------
times 510 - ($ - $$) db 0 ; fill rest of 512-byte sector with zeros
                          ; ($ - $$) calculates current size from 'org 0x7C00'
                          ; ensures boot sector is precisely 512 bytes long before the signature
dw 0xAA55                 ; Boot signature: 0xAA55 must be at offset 510 (0x1FE) of boot sector
                          ; BIOS verifies signature to consider the sector "bootable"