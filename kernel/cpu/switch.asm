; myos/kernel/cpu/switch.asm

bits 32

; Externally defined C functions we will call
extern schedule

; Functions we will make visible to the linker
global start_multitasking
global task_switch

; This function starts the very first task. It's only called once from kmain.
; It takes a pointer to the new task's cpu_state_t on the stack.
start_multitasking:
    ; Get the pointer to the cpu_state_t struct from the stack.
    mov ebx, [esp + 4]
    
    ; We are about to jump to a new task, so we can disable interrupts.
    ; The IRET instruction will re-enable them using the task's saved EFLAGS.
    cli

    ; Switch to the new task's stack.
    ; The 'useresp' field holds the correct stack pointer for the task.
    mov esp, [ebx + 44]  ; cpu_state_t.useresp is at offset 44

    ; Load all general-purpose registers from the cpu_state_t struct.
    ; We load them *before* the IRET so the new task starts with the correct state.
    mov edi, [ebx + 0]
    mov esi, [ebx + 4]
    mov ebp, [ebx + 8]
    ; We skip ESP because we've already loaded it.
    mov edx, [ebx + 20]
    mov ecx, [ebx + 24]
    mov eax, [ebx + 28]
    
    ; Load EBX last since we were using it as our pointer.
    mov ebx, [ebx + 16]

    ; Use IRET to jump to the new task. This is the magic step.
    ; It pops EIP, CS, and EFLAGS, effectively starting the new task.
    ; Because we are in Ring 0, it doesn't pop SS or ESP unless we're
    ; changing privilege levels, which we are not for these kernel tasks.
    iret

; This is the main context switching function, called by the timer IRQ handler.
; It takes a pointer to the *current* task's register state (the 'r' in timer_handler)
task_switch:
    ; The C IRQ handler already saved the general-purpose registers for us (pusha).
    ; Our job is to call the C scheduler to get the *next* task to run.
    
    ; Push the pointer to the current registers_t struct, which is the argument
    ; schedule() expects.
    push dword [esp + 4]
    call schedule
    add esp, 4  ; Clean up the argument from the stack.

    ; The C scheduler returns a pointer to the *new* task's cpu_state_t in EAX.
    ; Now, we restore the state from that struct.
    
    ; Before we switch, we MUST send the End-of-Interrupt signal to the PIC.
    ; If we don't, the PIC won't send any more timer interrupts.
    mov al, 0x20
    out 0xA0, al ; Send to Slave PIC first
    out 0x20, al ; Then to Master PIC

    ; Restore all general-purpose registers from the new task's saved state.
    ; Note: We do not touch ESP here. The 'popa' at the end of the irq_common_stub
    ; will restore the correct kernel stack pointer.
    mov edi, [eax + 0]
    mov esi, [eax + 4]
    mov ebp, [eax + 8]
    mov ebx, [eax + 16]
    mov edx, [eax + 20]
    mov ecx, [eax + 24]
    
    ; We need to be careful with ESP. The original registers_t struct is still on
    ; the stack. The 'popa' in irq_common_stub expects to see it. We need to
    ; replace the saved register values on the stack with the ones from the *new* task.
    ; 'r' (the pointer to registers_t) is at [esp].
    mov ebp, [esp]      ; Get pointer to registers_t
    mov [ebp + 28], eax ; eax
    mov [ebp + 24], ecx ; ecx
    mov [ebp + 20], edx ; edx
    mov [ebp + 16], ebx ; ebx
    ; skip original esp
    mov [ebp + 8], ebp ; ebp
    mov [ebp + 4], esi ; esi
    mov [ebp + 0], edi ; edi

    ; Finally, we update the instruction pointer (EIP) that the final 'iret'
    ; in irq_common_stub will use. This is how we jump to the new task.
    mov edi, [eax + 32] ; Get new EIP
    mov [ebp + 40], edi ; Save new EIP into the registers_t struct on the stack

    ; The 'eax' register is also part of the saved state, load it last.
    mov eax, [eax + 28]

    ; We are done. We simply return to 'irq_common_stub'. The stub will then
    ; execute 'popa' (restoring our newly loaded register values) and 'iret'
    ; (jumping to the new task's EIP).
    ret