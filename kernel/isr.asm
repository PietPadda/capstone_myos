; myos/kernel/isr.asm

; This macro creates a stub for an ISR that does not push an error code
%macro ISR_NOERRCODE 1
[GLOBAL isr%1]
isr%1:
    cli          ; Disable interrupts
    push 0       ; Push a dummy error code
    push %1      ; Push the interrupt number
    jmp isr_common_stub
%endmacro

; This macro creates a stub for an ISR that DOES push an error code
%macro ISR_ERRCODE 1
[GLOBAL isr%1]
isr%1:
    cli          ; Disable interrupts
    push %1      ; Push the interrupt number
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

; --- Common Assembly Stub ---
; All ISRs jump here after pushing their number and a (optional) error code
extern fault_handler ; This is our C-level handler
isr_common_stub:
    pusha        ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds   ; Lower 16 bits of ds register
    push eax     ; Save the data segment descriptor

    mov ax, 0x10 ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call fault_handler

    pop eax      ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa         ; Pop edi,esi,ebp,esp,ebx,edx,ecx,eax
    add esp, 8   ; Clean up the pushed error code and interrupt number
    iret         ; Return from interrupt