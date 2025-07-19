; myos/kernel/cpu/isr.asm
bits 32

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common_stub
%endmacro

; --- Define the 32 exception handlers ---
ISR_NOERRCODE 0  ; 0: Divide By Zero Exception
ISR_NOERRCODE 1  ; 1: Debug Exception
ISR_NOERRCODE 2  ; 2: Non Maskable Interrupt Exception
ISR_NOERRCODE 3  ; 3: Breakpoint Exception
ISR_NOERRCODE 4  ; 4: Into Detected Overflow Exception
ISR_NOERRCODE 5  ; 5: Out of Bounds Exception
ISR_NOERRCODE 6  ; 6: Invalid Opcode Exception
ISR_NOERRCODE 7  ; 7: No Coprocessor Exception
ISR_ERRCODE   8  ; 8: Double Fault Exception (with error code)
ISR_NOERRCODE 9  ; 9: Coprocessor Segment Overrun Exception
ISR_ERRCODE   10 ; 10: Bad TSS Exception (with error code)
ISR_ERRCODE   11 ; 11: Segment Not Present Exception (with error code)
ISR_ERRCODE   12 ; 12: Stack Fault Exception (with error code)
ISR_ERRCODE   13 ; 13: General Protection Fault Exception (with error code)
ISR_ERRCODE   14 ; 14: Page Fault Exception (with error code)
ISR_NOERRCODE 15 ; 15: Unassigned Exception
ISR_NOERRCODE 16 ; 16: Coprocessor Fault Exception
ISR_ERRCODE   17 ; 17: Alignment Check Exception (with error code)
ISR_NOERRCODE 18 ; 18: Machine Check Exception
ISR_NOERRCODE 19 ; 19: SIMD Fault Exception
; Interrupts 20-31 are reserved by Intel.
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; --- Define the 16 IRQ handlers ---
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword  0
    push dword  %2  ; Push the interrupt number
    jmp irq_common_stub
%endmacro

; IRQs 0-15. Remember we remapped them to 32-47.
IRQ 0, 32   ; Timer
IRQ 1, 33   ; Keyboard
IRQ 2, 34   ; Cascade
IRQ 3, 35   ; COM2
IRQ 4, 36   ; COM1
IRQ 5, 37   ; LPT2
IRQ 6, 38   ; Floppy Disk
IRQ 7, 39   ; LPT1
IRQ 8, 40   ; CMOS real-time clock
IRQ 9, 41   ; Free for peripherals
IRQ 10, 42  ; Free for peripherals
IRQ 11, 43  ; Free for peripherals
IRQ 12, 44  ; PS2 Mouse
IRQ 13, 45  ; FPU
IRQ 14, 46  ; Primary ATA Hard Disk
IRQ 15, 47  ; Secondary ATA Hard Disk

; --- Define our System Call Handler ---
global isr128
isr128:
    cli
    push dword 0    ; Push a dummy error code for stack consistency
    push dword 128  ; Push the interrupt number
    jmp syscall_common_stub

; --- Common Assembly Stub ---
; All ISRs jump here after pushing their number and a (optional) error code
extern fault_handler ; This is our C-level handler
isr_common_stub:
    ; 1. Save the general purpose registers
    pusha

    ; 2. Save the segment registers
    mov eax, ds
    push eax
    mov eax, es
    push eax
    mov eax, fs
    push eax
    mov eax, gs
    push eax

    ; 3. Load kernel segments for the C handler to use
    mov ax, 0x10  ; 0x10 is our kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 4. Push a pointer to the register block and call the C handler
    push esp
    call fault_handler

    ; 5. Clean up the pushed pointer from the stack
    add esp, 4

    ; 6. Restore the original segment registers
    pop eax
    mov gs, eax
    pop eax
    mov fs, eax
    pop eax
    mov es, eax
    pop eax
    mov ds, eax

    ; 7. Restore general purpose registers
    popa

    ; 8. Clean up error code & interrupt number, and return
    add esp, 8
    iret

; --- Common IRQ Stub ---
extern irq_handler ; Our new C-level IRQ handler
irq_common_stub:
    ; Save general purpose registers
    pusha

    ; Save segment registers
    mov eax, ds
    push eax
    mov eax, es
    push eax
    mov eax, fs
    push eax
    mov eax, gs
    push eax

    ; Load the kernel's data segment for C code
    mov ax, 0x10  ; 0x10 is our GDT data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push a pointer to the registers and call the C handler
    push esp
    call irq_handler
    add esp, 4 ; Clean up the pushed pointer

    ; Restore original segment registers
    pop eax
    mov gs, eax
    pop eax
    mov fs, eax
    pop eax
    mov es, eax
    pop eax
    mov ds, eax

    ; Restore general purpose registers
    popa
    add esp, 8 ; Clean up the error code and interrupt number
    iret       ; Atomically restores EFLAGS (which re-enables interrupts) and returns.


; --- Common Syscall Stub ---
extern syscall_handler ;our new C-level syscall handler
syscall_common_stub:
    ; Save general purpose registers
    pusha

    ; Save segment registers
    mov eax, ds
    push eax
    mov eax, es
    push eax
    mov eax, fs
    push eax
    mov eax, gs
    push eax

    ; Load the kernel's data segment for C code
    mov ax, 0x10  ; 0x10 is our GDT data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push a pointer to the registers and call the C handler
    push esp
    call syscall_handler
    add esp, 4 ; Clean up the pushed pointer

    ; Restore original segment registers
    pop eax
    mov gs, eax
    pop eax
    mov fs, eax
    pop eax
    mov es, eax
    pop eax
    mov ds, eax

    ; Restore general purpose registers
    popa
    add esp, 8 ; Clean up the error code and interrupt number
    iret       ; Atomically restores EFLAGS and returns.