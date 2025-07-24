; myos/kernel/cpu/process_asm.asm

bits 32
global start_multitasking

; Jumps to the first task by loading its state and executing iret
start_multitasking:
    ; The C code calls this function, so the pointer to the cpu_state 
    ; struct is on the stack at [esp + 4]. We'll get it into EBX.
    mov ebx, [esp + 4]

    cli ; Disable interrupts while we are switching contexts

    ; Load all general-purpose registers from the new task's saved state.
    ; The offsets are relative to the start of the cpu_state_t struct.
    mov eax, [ebx + 28]  ; EAX is at offset 28
    mov ecx, [ebx + 24]  ; ECX
    mov edx, [ebx + 20]  ; EDX
    mov ebp, [ebx + 8]   ; EBP
    mov esi, [ebx + 4]   ; ESI
    mov edi, [ebx + 0]   ; EDI
    
    ; We switch to the new task's stack. The stack pointer is stored in the
    ; 'useresp' field of the cpu_state structure.
    mov esp, [ebx + 44]  ; useresp is at offset 44

    ; Now that we are safely on the new task's stack, we can push the
    ; five values that the IRET instruction needs to start the task.
    push dword [ebx + 48]  ; SS       (offset 48)
    push dword [ebx + 44]  ; ESP      (the value of useresp again)
    push dword [ebx + 40]  ; EFLAGS   (offset 40)
    push dword [ebx + 36]  ; CS       (offset 36)
    push dword [ebx + 32]  ; EIP      (offset 32)

    ; Finally, load EBX itself, as we were using it as our base pointer.
    mov ebx, [ebx + 16]  ; EBX is at offset 16

    ; We are ready. IRET will pop EIP, CS, EFLAGS, ESP, and SS from the stack
    ; and begin executing the new task. The other registers are already loaded.
    iret