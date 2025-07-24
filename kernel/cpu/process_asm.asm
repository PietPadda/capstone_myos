; myos/kernel/cpu/process_asm.asm

bits 32
global start_multitasking

start_multitasking:
    ; The C code provides a pointer to cpu_state_t at [esp + 4]. Get it.
    mov ebx, [esp + 4]
    cli 

    ; Switch to the new task's stack *first*.
    mov esp, [ebx + 44]   ; Load useresp into ESP (offset 44)

    ; On the new stack, push the 3-word IRET frame for a same-privilege return.
    push dword [ebx + 40] ; EFLAGS
    push dword [ebx + 36] ; CS
    push dword [ebx + 32] ; EIP

    ; Load the general purpose registers. EBX still holds the pointer.
    mov eax, [ebx + 28]
    mov ecx, [ebx + 24]
    mov edx, [ebx + 20]
    mov ebp, [ebx + 8]
    mov esi, [ebx + 4]
    mov edi, [ebx + 0]

    ; Load EBX itself last.
    mov ebx, [ebx + 16]

    ; We are ready. IRET will pop EIP, CS, and EFLAGS and start the task.
    iret