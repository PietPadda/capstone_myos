; myos/kernel/cpu/switch.asm

bits 32

; Externally defined C functions we will call
extern schedule
; extern qemu_debug_string ; Make our debug function visible
extern tss_entry ; Expose TSS in memory to assembly

; A string to print from assembly
; switch_msg: db 'task_switch: Entered assembly function.', 0

; Functions we will make visible to the linker
global start_multitasking
global task_switch

; This function starts the very first task. It's only called once from kmain.
; It takes a pointer to the new task's cpu_state_t on the stack.
start_multitasking:
    ; Get the pointer to the cpu_state_t struct from the stack.
    mov ebx, [esp + 4]

    ; Update the TSS to point to this new task's kernel stack.
    ; The kernel_stack pointer is part of the task_struct_t, not the cpu_state_t.
    ; We need to calculate its offset from the cpu_state_t pointer.
    ; In task_struct_t: `cpu_state` is at offset 60. `kernel_stack` is at offset 52.
    ; So, kernel_stack is at [ebx - 8].
    mov eax, [ebx - 8]      ; eax = new_task->kernel_stack
    add eax, 4096           ; eax = top of the 4K kernel stack
    mov [tss_entry + 4], eax ; tss_entry.esp0 = eax

    ; Load the new task's page directory
    mov eax, [ebx + 48] ; cpu_state.cr3 is at offset 48
    mov cr3, eax
    
    ; We are about to jump to a new task, so we can disable interrupts.
    ; The IRET instruction will re-enable them using the task's saved EFLAGS.
    cli

    ; Switch to the new task's stack.
    mov esp, [ebx + 40]  ; cpu_state_t.esp is at offset 40

    ; Push the IRET frame onto the new stack in the correct order.
    ; For a privilege change, IRET pops EIP, CS, EFLAGS, ESP, and SS.
    ; We push them in the reverse order.
    push dword [ebx + 44]   ; SS
    push dword [ebx + 40]   ; ESP
    push dword [ebx + 36]   ; EFLAGS
    push dword [ebx + 32]   ; CS
    push dword [ebx + 28]   ; EIP

    ; Load all general-purpose registers from the cpu_state_t struct.
    ; We load them *before* the IRET so the new task starts with the correct state.
    mov edi, [ebx + 0]
    mov esi, [ebx + 4]
    mov ebp, [ebx + 8]
    ; We skip ESP because we've already loaded it.
    mov edx, [ebx + 16]
    mov ecx, [ebx + 20]
    mov eax, [ebx + 24]
    
    ; Load EBX last since we were using it as our pointer.
    mov ebx, [ebx + 12]

    ; Use IRET to jump to the new task. This is the magic step.
    iret

; This is the main context switching function, called by the timer IRQ handler.
; It takes a pointer to the *current* task's register state (the 'r' in timer_handler)
task_switch:
    ; --- New Debug Print ---
    ;pusha
    ;push switch_msg
    ;call qemu_debug_string
    ;add esp, 4
    ;popa
    ; --- End New Debug Print ---

    ; Set up a standard C-style stack frame
    push ebp
    mov ebp, esp

    ; Call the C scheduler. Its argument 'r' is on our stack at [ebp + 8]
    push dword [ebp + 8]
    call schedule
    add esp, 4      ; Clean up argument
    
    ; Immediately save the return value from schedule (in EAX) into a safe register.
    mov ecx, eax        ; ecx now holds the pointer to the new task's state.

    ; Now we can safely use EAX for other things, like sending the EOI.
    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    ; Get the pointer to the on-stack registers_t ('r')
    mov ebx, [ebp + 8]

    ; --- Overwrite the on-stack interrupt frame with the new task's state ---
    ; We use EDX as a temporary register to move data.
    ; Format: mov edx, [new_state + offset]; mov [r + offset], edx
    mov edx, [ecx + 0]   ; edi
    mov [ebx + 16], edx
    mov edx, [ecx + 4]   ; esi
    mov [ebx + 20], edx
    mov edx, [ecx + 8]   ; ebp
    mov [ebx + 24], edx
    ; NOTE: We do not restore ESP from the struct. `popa` handles it, and
    ; the `iret` frame has its own stack pointer.

    mov edx, [ecx + 12]  ; ebx
    mov [ebx + 32], edx
    mov edx, [ecx + 16]  ; edx
    mov [ebx + 36], edx
    mov edx, [ecx + 20]  ; ecx
    mov [ebx + 40], edx
    mov edx, [ecx + 24]  ; eax
    mov [ebx + 44], edx
    
    ; Update the IRET frame on the stack
    mov edx, [ecx + 28]  ; eip
    mov [ebx + 56], edx
    mov edx, [ecx + 32]  ; cs
    mov [ebx + 60], edx
    mov edx, [ecx + 36]  ; eflags
    mov [ebx + 64], edx

    ; We must also update the user stack pointer and segment for the IRET.
    ; These are at offsets 68 and 72 in the registers_t struct.
    mov edx, [ecx + 40]  ; new_task->cpu_state.esp
    mov [ebx + 68], edx  ; r->useresp
    mov edx, [ecx + 44]  ; new_task->cpu_state.ss
    mov [ebx + 72], edx  ; r->ss

    ; --- THIS IS THE MAGIC ---
    ; Load the physical address of the new task's page directory into CR3.
    mov edx, [ecx + 48]  ; new_task->cpu_state.cr3
    mov cr3, edx

    ; We are done. Restore our stack frame and return to irq_common_stub.
    mov esp, ebp
    pop ebp
    ret