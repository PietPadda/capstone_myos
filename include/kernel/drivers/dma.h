// myos/include/kernel/drivers/dma.h

#ifndef DMA_H
#define DMA_H

#include <kernel/types.h>

// I/O ports for the 8237 DMA controller
#define DMA_MASK_REG      0x0A // Write-only
#define DMA_MODE_REG      0x0B // Write-only
#define DMA_CLEAR_FF_REG  0x0C // Write-only

#define DMA_ADDR_CH1      0x02 // Channel 1 Address Port
#define DMA_COUNT_CH1     0x03 // Channel 1 Count Port
#define DMA_PAGE_ADDR_CH1 0x83 // Channel 1 Page Address Register

// Prepares a DMA transfer for a given channel.
void dma_prepare_transfer(uint8_t channel, uint8_t mode, uint32_t addr, uint32_t size);

#endif // DMA_H