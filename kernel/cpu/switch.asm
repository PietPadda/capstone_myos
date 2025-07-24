; myos/kernel/cpu/switch.asm

bits 32

global task_switch
extern schedule

; This function is called by the C timer_handler.
; It receives a pointer to the interrupt register frame 'r'.
task_switch:
    ; [esp + 4] holds the pointer 'r'

    ; Call the C scheduler. It takes 'r' as an argument and returns
    ; a pointer to the new task's cpu_state in the EAX register.
    push dword [esp + 4]
    call schedule
    add esp, 4

    ; EAX now holds a pointer to the cpu_state of the task to switch to.

    ; Send End-of-Interrupt signal to the PIC.
    mov al, 0x20
    out 0x20, al

    ; Load the general-purpose registers for the new task from the state
    ; pointed to by EAX. We load ESP last.
    mov ebx, [eax + 16]  ; Offset of ebx in cpu_state_t
    mov ecx, [eax + 24]  ; Offset of ecx
    mov edx, [eax + 20]  ; Offset of edx
    mov esi, [eax + 4]   ; Offset of esi
    mov edi, [eax + 0]   ; Offset of edi
    mov ebp, [eax + 8]   ; Offset of ebp

    ; Switch to the new task's stack. useresp is where the task's
    ; own stack pointer is stored.
    mov esp, [eax + 44]  ; Offset of useresp

    ; Push the IRET frame onto the NEW stack.
    push dword [eax + 48]  ; ss
    push dword [eax + 44]  ; useresp
    push dword [eax + 40]  ; eflags
    push dword [eax + 36]  ; cs
    push dword [eax + 32]  ; eip

    ; Load EAX itself. We do this last because we were using it as our base pointer.
    mov eax, [eax + 28]  ; Offset of eax

    ; Return from the interrupt. This will pop the 5 values we just pushed
    ; and begin executing the new task.
    iret