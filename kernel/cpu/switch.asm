; myos/kernel/cpu/switch.asm

bits 32

; External C functions and variables we need
extern schedule       ; Our simple C scheduler
extern current_task   ; Pointer to the current task's PCB

; Functions we make visible to the linker
global start_multitasking
global task_switch

; This function starts the very first task. It's only called once.
start_multitasking:
    mov ebx, [esp + 4]  ; Get pointer to the first task's cpu_state_t
    cli

    ; Switch to the new task's stack.
    ; We get the stack pointer from the useresp field.
    mov esp, [ebx + 44]

    ; Push the IRET frame onto the new stack.
    push dword [ebx + 40]   ; EFLAGS
    push dword [ebx + 36]   ; CS
    push dword [ebx + 32]   ; EIP

    ; Load all general-purpose registers from the cpu_state_t struct.
    mov edi, [ebx + 0]
    mov esi, [ebx + 4]
    mov ebp, [ebx + 8]
    mov edx, [ebx + 20]
    mov ecx, [ebx + 24]
    mov eax, [ebx + 28]
    mov ebx, [ebx + 16] ; Load EBX last

    ; Jump to the new task. This does not return.
    iret

; This is the new, correct context switching function.
; It's called by the timer handler. The pointer to the interrupt frame ('r')
; is on the stack, but we will not use it directly anymore. Instead, we use
; pusha/popa and the ESP register to manage state.
task_switch:
    ; --- 1. Save Old State ---
    ; The CPU has already disabled interrupts and pushed EIP, CS, EFLAGS.
    ; The IRQ stub in isr.asm has pushed the int_no and a dummy error code.
    pusha                 ; Save all general purpose registers (eax, ecx, etc.).

    ; Save the current stack pointer (ESP) into the old task's PCB.
    mov eax, [current_task]
    mov [eax + 44], esp   ; old_task->cpu_state.useresp = esp

    ; --- 2. Choose New State ---
    call schedule         ; Call the C scheduler to update 'current_task'.

    ; --- 3. Restore New State ---
    ; Load the stack pointer (ESP) from the new task's PCB.
    ; This is the moment we officially switch stacks.
    mov eax, [current_task]
    mov esp, [eax + 44]   ; esp = new_task->cpu_state.useresp

    ; Send End-of-Interrupt to the PIC. This is the safest place to do it,
    ; after all state is saved and we're committed to the switch.
    mov al, 0x20
    out 0x20, al

    ; Restore all general purpose registers from the new task's stack.
    popa

    ; The IRQ stub pushed these; we must pop them to clean up the stack.
    add esp, 8

    ; Return from the interrupt, loading the new EIP, CS, and EFLAGS.
    iret