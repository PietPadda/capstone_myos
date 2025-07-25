; myos/kernel/cpu/switch.asm

bits 32

; Externally defined C functions we will call
extern schedule
extern qemu_debug_string
extern qemu_debug_memdump

; Functions we will make visible to the linker
global start_multitasking
global task_switch

; Add a new section for our debug strings
section .data
    str_start_multi: db 'switch.asm: entered start_multitasking.', 10, 0 ; 10 is newline
    str_task_switch: db 'switch.asm: entered task_switch.', 10, 0
    ; Add a new string for our new checkpoint
    str_task_switch_ret: db 'switch.asm: returned from schedule.', 10, 0

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

    ; Set up a standard C-style stack frame
    push ebp
    mov ebp, esp

    ; Call the C scheduler. Its argument 'r' is on our stack at [ebp + 8]
    push dword [ebp + 8]
    call schedule
    add esp, 4      ; Clean up argument

    ; --- Checkpoint ---
    pushad
    push str_task_switch_ret
    call qemu_debug_string
    add esp, 4
    popad
    
    ; The C scheduler returned a pointer to the NEW task's cpu_state_t in EAX.

    ; Get the pointer to the on-stack registers_t ('r') from our arguments.
    mov ebx, [ebp + 8]  ; ebx = r (points to the stack frame to be modified)
    ; Get the pointer to the new task's state (returned from schedule).
    mov ecx, eax        ; ecx = new_state

    ; --- NEW MEMORY DUMPS ---
    pushad
    ; Dump the new state we are about to load (task_b's cpu_state_t)
    push 52             ; Arg 2: size of cpu_state_t
    push ecx            ; Arg 1: address of new_state
    call qemu_debug_memdump
    add esp, 8          ; Clean up 2 arguments

    ; Dump the on-stack frame we are about to overwrite (task_a's registers_t)
    push 76             ; Arg 2: size of registers_t
    push ebx            ; Arg 1: address of r
    call qemu_debug_memdump
    add esp, 8          ; Clean up 2 arguments
    popad

    ; Send End-of-Interrupt signal to the PICs
    mov al, 0x20
    out 0xA0, al ; Slave PIC
    out 0x20, al ; Master PIC

    ; --- Overwrite the on-stack interrupt frame with the new task's state ---
    ; We use EDX as a temporary register to move data.
    ; Format: mov edx, [new_state + offset]; mov [r + offset], edx
    mov edx, [ecx + 0]   ; edi
    mov [ebx + 16], edx
    mov edx, [ecx + 4]   ; esi
    mov [ebx + 20], edx
    mov edx, [ecx + 8]   ; ebp
    mov [ebx + 24], edx
    mov edx, [ecx + 44]  ; useresp (the new task's stack pointer)
    mov [ebx + 28], edx  ; This updates the esp that popa will see
    mov edx, [ecx + 16]  ; ebx
    mov [ebx + 32], edx
    mov edx, [ecx + 20]  ; edx
    mov [ebx + 36], edx
    mov edx, [ecx + 24]  ; ecx
    mov [ebx + 40], edx
    mov edx, [ecx + 28]  ; eax
    mov [ebx + 44], edx
    
    ; --- This is the most important part: update the return address for iret ---
    mov edx, [ecx + 32]  ; eip
    mov [ebx + 56], edx
    mov edx, [ecx + 36]  ; cs
    mov [ebx + 60], edx
    mov edx, [ecx + 40]  ; eflags
    mov [ebx + 64], edx

    ; We are done. Restore our stack frame and return to irq_common_stub.
    mov esp, ebp
    pop ebp
    ret