// myos/kernel/disk.c

#include "disk.h"
#include "io.h" // for port_word_in

// Helper function to wait for the disk to be ready.
static void wait_disk_ready() {
    // Wait until the BSY (busy) bit is cleared in the status port.
    while (port_byte_in(0x1F7) & 0x80) {
        // Loop and do nothing while the disk is busy.
    }
}

void read_disk_sector(uint32_t lba, uint8_t* buffer) {
    wait_disk_ready();

    // Wait for the drive to settle by reading the status port 4 times.
    // This is the "400ns delay".
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);

    // Send drive select and highest LBA bits
    port_byte_out(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));

    // Send the number of sectors to read (1)
    port_byte_out(0x1F2, 1);

    // Send the LBA address bytes
    port_byte_out(0x1F3, (uint8_t)lba);
    port_byte_out(0x1F4, (uint8_t)(lba >> 8));
    port_byte_out(0x1F5, (uint8_t)(lba >> 16));


    // Send the READ SECTORS command
    port_byte_out(0x1F7, 0x20);

    wait_disk_ready();

    // Read 256 words (512 bytes) from the data port into the buffer
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = port_word_in(0x1F0);
    }
}