; myos/kernel/cpu/switch.asm

bits 32

global task_switch
extern schedule

task_switch:
    ; Call the C scheduler. It returns a pointer to the new task's state in EAX.
    push dword [esp + 4]
    call schedule
    add esp, 4

    ; EAX now holds a pointer to the cpu_state of the task to switch to.

    ; Send End-of-Interrupt signal to the PIC.
    mov al, 0x20
    out 0x20, al

    ; Switch to the new task's stack. EAX holds the pointer.
    mov esp, [eax + 44]  ; Offset of useresp

    ; Push the 3-word IRET frame onto the NEW stack.
    push dword [eax + 40]  ; eflags
    push dword [eax + 36]  ; cs
    push dword [eax + 32]  ; eip

    ; Load the general-purpose registers for the new task.
    mov ebx, [eax + 16]
    mov ecx, [eax + 24]
    mov edx, [eax + 20]
    mov esi, [eax + 4]
    mov edi, [eax + 0]
    mov ebp, [eax + 8]

    ; Load EAX itself last, since we used it as the pointer.
    mov eax, [eax + 28]

    ; Return from the interrupt. This will pop EIP, CS, EFLAGS and
    ; begin executing the new task.
    iret