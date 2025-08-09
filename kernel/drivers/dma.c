// myos/kernel/drivers/dma.c

#include <kernel/drivers/dma.h>
#include <kernel/io.h>

// Prepares a DMA transfer for a given channel.
// NOTE: This only works for channels 0-3 (8-bit transfers).
void dma_prepare_transfer(uint8_t channel, uint8_t mode, uint32_t addr, uint32_t size) {
    // The DMA controller can only access the first 16MB of RAM.
    if (addr >= 0x1000000) {
        return; // Address is out of range
    }

    // Disable the DMA channel
    port_byte_out(DMA_MASK_REG, 0x04 | channel);

    // Clear the byte pointer flip-flop
    port_byte_out(DMA_CLEAR_FF_REG, 0x00);

    // Set the transfer mode
    //    - 0x48 = Single cycle, auto-initialize, memory-to-device
    port_byte_out(DMA_MODE_REG, mode | channel);

    // Send the physical address
    port_byte_out(DMA_ADDR_CH1, (uint8_t)(addr & 0xFF));        // Low byte
    port_byte_out(DMA_ADDR_CH1, (uint8_t)((addr >> 8) & 0xFF)); // High byte

    // Send the page number (upper 8 bits of the address)
    port_byte_out(DMA_PAGE_ADDR_CH1, (uint8_t)((addr >> 16) & 0xFF));

    // Send the transfer size (must be size-1)
    uint32_t count = size - 1;
    port_byte_out(DMA_COUNT_CH1, (uint8_t)(count & 0xFF));        // Low byte
    port_byte_out(DMA_COUNT_CH1, (uint8_t)((count >> 8) & 0xFF)); // High byte

    // Enable the DMA channel
    port_byte_out(DMA_MASK_REG, channel);
}