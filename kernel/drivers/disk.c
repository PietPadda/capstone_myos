// myos/kernel/drivers/disk.c

#include <kernel/disk.h>
#include <kernel/io.h> // for port_word_in
#include <kernel/pmm.h>  // the PMM
#include <kernel/string.h> // memcpy

// A single, shared 4KB buffer for all disk I/O operations.
// It's allocated in a known physical location.
static uint8_t* disk_io_buffer = NULL;

// This should be called once during kernel initialization.
void disk_init() {
    // Allocate one frame from the PMM. Since it will be below 4MB,
    // its physical address will equal its virtual address.
    disk_io_buffer = pmm_alloc_frame();
}

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

    // Wait for the drive to settle by reading the status port 4 times.
    // This is the "400ns delay".
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);

    // Send drive select and highest LBA bits
    // We pass the PHYSICAL address of our shared buffer to the ATA driver.
    // The ATA command requires a physical address for DMA.
    port_byte_out(0x1F6, 0xE0 | ((uint32_t)disk_io_buffer >> 24) & 0x0F);

    // Add a small delay after selecting the drive. This gives the
    // controller time to get ready for the rest of the command.
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);

    // Send the number of sectors to read (1)
    port_byte_out(0x1F2, 1);

    // Send the LBA address bytes
    port_byte_out(0x1F3, (uint8_t)lba);
    port_byte_out(0x1F4, (uint8_t)(lba >> 8));
    port_byte_out(0x1F5, (uint8_t)(lba >> 16));
    
    // Send the READ SECTORS command
    port_byte_out(0x1F7, 0x20);

    // A more robust wait loop that also checks for errors.
    while (1) {
        uint8_t status = port_byte_in(0x1F7);

        if (status & 0x01) { // Is Bit 0 (ERR) set?
            // In a real OS, we'd read the error port and handle it.
            // For now, we just stop.
            return;
        }

        if (!(status & 0x80)) { // Is Bit 7 (BSY) clear?
            if (status & 0x08) { // Is Bit 3 (DRQ) set?
                // Yes, ready to read data!
                break;
            }
        }
    }

    // Read 256 words (512 bytes) from the data port into our shared buffer
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)disk_io_buffer)[i] = port_word_in(0x1F0);
    }

    // Now, copy the 512 bytes from our shared I/O buffer to the
    // virtual address buffer that the caller provided.
    memcpy(buffer_out, disk_io_buffer, 512);

    // Reading the status port after a read command acknowledges the interrupt,
    // allowing the next command to be processed.
    port_byte_in(0x1F7);
}