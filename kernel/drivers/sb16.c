// myos/kernel/drivers/sb16.c

#include <kernel/drivers/sb16.h>
#include <kernel/io.h>
#include <kernel/vga.h> // For printing status messages

// Helper function to write a command/data to the DSP
static void sb16_dsp_write(uint8_t value) {
    // Wait until the DSP is ready to accept a command
    while (port_byte_in(SB16_DSP_WRITE_STATUS) & 0x80);
    // Send the command
    port_byte_out(SB16_DSP_WRITE, value);
}

// Helper function to read data from the DSP
static uint8_t sb16_dsp_read() {
    // Wait until the DSP has data for us
    while (!(port_byte_in(SB16_DSP_READ_STATUS) & 0x80));
    // Read the data
    return port_byte_in(SB16_DSP_READ);
}

// Resets the DSP. Returns 1 on success, 0 on failure.
static int sb16_reset_dsp() {
    // Write 1 to the reset port
    port_byte_out(SB16_DSP_RESET, 1);
    
    // Wait for at least 3 microseconds. We'll just do a small busy-wait loop.
    for (volatile int i = 0; i < 1000; i++);

    // Write 0 back to the reset port
    port_byte_out(SB16_DSP_RESET, 0);

    // Wait for the acknowledgment byte (0xAA) from the DSP
    uint8_t ack = sb16_dsp_read();
    if (ack == 0xAA) {
        return 1; // Success
    }

    return 0; // Failure
}

// Main initialization function for the driver
void sb16_init() {
    print_string("Initializing Sound Blaster 16...\n");
    if (sb16_reset_dsp()) {
        print_string("  DSP reset successful!\n");
    } else {
        print_string("  DSP reset failed. Card not found or not responding.\n");
    }
}