; myos/kernel/cpu/paging_asm.asm

bits 32

global load_page_directory
global enable_paging

; Loads the physical address of the page directory into the CR3 register.
; The address is passed in on the stack.
load_page_directory:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

; Enables the paging bit in the CR0 register.
enable_paging:
    mov eax, cr0
    or eax, 0x80000000 ; Set bit 31 (PG)
    mov cr0, eax
    ; Immediately jump to a label to flush the CPU's prefetch queue.
    ; This is required by the Intel SDM after enabling paging.
    jmp .flush_pipe

.flush_pipe:
    ret