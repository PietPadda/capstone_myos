// myos/kernel/disk.c

#include <kernel/disk.h>
#include <kernel/io.h> // for port_word_in

// A more robust helper function to wait for the disk to be ready.
static void wait_disk_ready() {
    // Wait until the controller is ready for a new command.
    // This checks that BSY (Bit 7) is 0 and RDY (Bit 6) is 1.
    while ((port_byte_in(0x1F7) & 0xC0) != 0x40) {
        // Loop and do nothing.
    }
}

void read_disk_sector(uint32_t lba, uint8_t* buffer) {
    port_byte_out(0xE9, 'A'); // START of disk read
    wait_disk_ready();

    // Wait for the drive to settle by reading the status port 4 times.
    // This is the "400ns delay".
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);
    port_byte_in(0x1F7);

    // Send drive select and highest LBA bits
    port_byte_out(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));

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
    port_byte_out(0xE9, 'B'); // Read command sent


    // A more robust wait loop that also checks for errors.
    while (1) {
        uint8_t status = port_byte_in(0x1F7);

        if (status & 0x01) { // Is Bit 0 (ERR) set?
            port_byte_out(0xE9, 'X'); // Print 'X' for ERROR
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
    port_byte_out(0xE9, 'C'); // Disk is ready (DRQ is set)

    // Read 256 words (512 bytes) from the data port into the buffer
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = port_word_in(0x1F0);
    }
    port_byte_out(0xE9, 'D'); // FINISHED disk read

    // Reading the status port after a read command acknowledges the interrupt,
    // allowing the next command to be processed.
    port_byte_in(0x1F7);
    port_byte_out(0xE9, 'E'); // debug
}