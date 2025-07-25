; myos/kernel/cpu/switch.asm

bits 32

; Externally defined C functions we will call
extern schedule
extern qemu_debug_string

; Functions we will make visible to the linker
global start_multitasking
global task_switch

; Add a new section for our debug strings
section .data
    str_start_multi: db 'switch.asm: entered start_multitasking.', 10, 0 ; 10 is newline
    str_task_switch: db 'switch.asm: entered task_switch.', 10, 0

; This function starts the very first task. It's only called once from kmain.
; It takes a pointer to the new task's cpu_state_t on the stack.
start_multitasking:
    ; --- Checkpoint ---
    pushad                      ; Save all general purpose registers
    push str_start_multi        ; Push the string argument for our C function
    call qemu_debug_string      ; Call the C function
    add esp, 4                  ; Clean the argument from the stack
    popad                       ; Restore all registers

    ; Get the pointer to the cpu_state_t struct from the stack.
    mov ebx, [esp + 4]
    
    ; We are about to jump to a new task, so we can disable interrupts.
    ; The IRET instruction will re-enable them using the task's saved EFLAGS.
    cli

    ; Switch to the new task's stack.
    mov esp, [ebx + 44]  ; cpu_state_t.useresp is at offset 44

    ; Push the IRET frame onto the new stack in the correct order for IRET.
    ; IRET pops EIP, CS, EFLAGS. So we must push them in reverse order.
    push dword [ebx + 40]   ; EFLAGS
    push dword [ebx + 36]   ; CS
    push dword [ebx + 32]   ; EIP

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
    iret

; This is the main context switching function, called by the timer IRQ handler.
; It takes a pointer to the *current* task's register state (the 'r' in timer_handler)
task_switch:
    ; --- Checkpoint ---
    pushad
    push str_task_switch
    call qemu_debug_string
    add esp, 4
    popad

    push ebp
    mov ebp, esp

    ; Call the scheduler. The pointer 'r' is our first argument at [ebp + 8]
    push dword [ebp + 8]
    call schedule
    add esp, 4      ; Clean up stack

    ; EAX now holds a pointer to the *next* task's cpu_state_t.
    ; Let's call this 'new_state'.

    ; Send EOI to PICs
    mov al, 0x20
    out 0xA0, al ; Slave
    out 0x20, al ; Master

    ; Overwrite the saved registers on the stack frame created by the interrupt.
    ; The pointer to this frame ('r') is at [ebp + 8].
    ; The pointer to the new state is in EAX.
    mov ebx, [ebp + 8]  ; ebx = r
    mov ecx, eax        ; ecx = new_state

    ; For each register, load from new_state and store into the on-stack frame 'r'.
    ; We use EDX as a temporary register.
    mov edx, [ecx + 0]  ; edx = new_state->edi
    mov [ebx + 28], edx ; r->edi = edx  (Note: edi is at offset 28 in registers_t)
    mov edx, [ecx + 4]  ; edx = new_state->esi
    mov [ebx + 24], edx ; r->esi = edx
    mov edx, [ecx + 8]  ; edx = new_state->ebp
    mov [ebx + 20], edx ; r->ebp = edx
    ; We must also update the stack pointer in the frame!
    mov edx, [ecx + 44] ; edx = new_state->useresp
    mov [ebx + 16], edx ; r->esp = edx
    mov edx, [ecx + 16] ; edx = new_state->ebx
    mov [ebx + 12], edx ; r->ebx = edx
    mov edx, [ecx + 20] ; edx = new_state->edx
    mov [ebx + 8], edx  ; r->edx = edx
    mov edx, [ecx + 24] ; edx = new_state->ecx
    mov [ebx + 4], edx  ; r->ecx = edx
    mov edx, [ecx + 28] ; edx = new_state->eax
    mov [ebx + 0], edx  ; r->eax = edx

    ; Finally, update the instruction pointer (EIP) that 'iret' will use.
    mov edx, [ecx + 32] ; edx = new_state->eip
    mov [ebx + 40], edx ; r->eip = edx

    ; Restore stack frame and return to irq_common_stub.
    ; The stub will then `popa` our modified registers and `iret` to the new task.
    mov esp, ebp
    pop ebp
    ret