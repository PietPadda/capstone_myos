; myos/userspace/user_entry.asm
bits 32

; Tell the linker that the following functions are defined elsewhere
extern user_program_main

; Make our new entry point visible to the linker
global _start_user_program

_start_user_program:
    ; We are in a special state. The kernel placed argc and argv on the stack,
    ; but it didn't use a 'call' instruction. The stack looks like this:
    ; [argv pointer]
    ; [argc]
    ; This is the reverse order of a standard C function call.

    ; Push the arguments onto the stack in the correct order for the C function.
    ; We are just re-arranging the data that is already there.
    mov eax, [esp]      ; Get argc from the top of the stack
    mov ebx, [esp + 4]  ; Get the argv pointer from the next position
    
    push ebx            ; Push the argv pointer (right-to-left)
    push eax            ; Push argc (right-to-left)

    ; Call our C main function
    call user_program_main

    ; Our C function has returned. We must now exit the program cleanly
    ; by performing the syscall directly. The syscall number for exit is 3.
    mov eax, 3          ; Put the syscall number for 'exit' into EAX
    int 0x80            ; Trigger the system call interrupt