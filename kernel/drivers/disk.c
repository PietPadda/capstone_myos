// myos/kernel/drivers/disk.c

#include <kernel/disk.h>
#include <kernel/io.h> // for port_word_in
#include <kernel/string.h> // memcpy

// This label is defined in our new dma_buffer.asm file.
// The linker will guarantee it's at a valid, DMA-safe physical address.
extern uint8_t dma_buffer[];

// A more robust helper function to wait for the disk to be ready.
static void wait_disk_ready() {
    // Wait until the controller is ready for a new command.
    // This checks that BSY (Bit 7) is 0 and RDY (Bit 6) is 1.
    while ((port_byte_in(0x1F7) & 0xC0) != 0x40) {
        // Loop and do nothing.
    }
}

// This function is now updated to use the shared I/O buffer
void read_disk_sector(uint32_t lba, uint8_t* buffer_out) {
    wait_disk_ready();
    port_byte_out(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    port_byte_out(0x1F2, 1);
    port_byte_out(0x1F3, (uint8_t)lba);
    port_byte_out(0x1F4, (uint8_t)(lba >> 8));
    port_byte_out(0x1F5, (uint8_t)(lba >> 16));
    port_byte_out(0x1F7, 0x20);

    wait_disk_ready();

    // Read the data from the hardware into our safe, static DMA buffer.
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)dma_buffer)[i] = port_word_in(0x1F0);
    }
    
    // Now, copy the data from the safe buffer to the caller's virtual memory buffer.
    memcpy(buffer_out, dma_buffer, 512);
}