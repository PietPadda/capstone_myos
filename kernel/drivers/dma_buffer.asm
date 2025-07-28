; myos/kernel/drivers/dma_buffer.asm
bits 32

section .bss 
align 4
global dma_buffer 
dma_buffer:
    resb 512