// myos/kernel/drivers/sb16.c

#include <kernel/drivers/sb16.h>
#include <kernel/io.h>
#include <kernel/vga.h> // For printing status messages
#include <kernel/pmm.h>           // For pmm_alloc_frame()
#include <kernel/drivers/dma.h>   // For our new DMA functions
#include <kernel/timer.h> // For sleep()

static uint8_t* dma_buffer = NULL; // A place to store our DMA buffer address

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

        // Allocate a 4KB page-aligned buffer from low memory for DMA
        dma_buffer = (uint8_t*)pmm_alloc_frame();
        print_string("  DMA buffer allocated at physical address: ");
        print_hex((uint32_t)dma_buffer);
        print_string("\n");

        // Prepare DMA Channel 1 for an 8-bit, single-cycle transfer
        // 0x48 = Single Cycle, Auto-initialize, Write transfer (to device)
        dma_prepare_transfer(1, 0x48, (uint32_t)dma_buffer, PMM_FRAME_SIZE);
        print_string("  DMA channel 1 programmed for transfer.\n");

    } else {
        print_string("  DSP reset failed. Card not found or not responding.\n");
    }
}

// Plays a square wave of a given frequency and duration.
void sb16_play_sound(uint16_t frequency, uint16_t duration_ms) {
    // A sample rate of 22050 Hz is well-supported and a good compromise.
    const uint16_t sample_rate = 22050;

    // Generate the square wave data and fill the DMA buffer
    uint32_t period = sample_rate / frequency;
    uint32_t half_period = period / 2;
    for (uint32_t i = 0; i < PMM_FRAME_SIZE; i++) {
        // The SB16 uses unsigned 8-bit audio data.
        // 0xFF is max amplitude, 0x00 is min amplitude.
        if ((i % period) < half_period) {
            dma_buffer[i] = 0xFF; // High
        } else {
            dma_buffer[i] = 0x00; // Low
        }
    }

    // Turn the speaker on (required before playback)
    sb16_dsp_write(0xD1);

    // Set the sample rate
    sb16_dsp_write(0x41); // Command to set output sample rate
    sb16_dsp_write((uint8_t)((sample_rate >> 8) & 0xFF)); // High byte
    sb16_dsp_write((uint8_t)(sample_rate & 0xFF));        // Low byte

    // Command the DSP to start the DMA transfer
    uint16_t length = PMM_FRAME_SIZE - 1;
    sb16_dsp_write(0xC6); // 8-bit auto-initialized DMA output
    sb16_dsp_write(0x00); // Transfer mode (mono)
    sb16_dsp_write((uint8_t)(length & 0xFF));        // Low byte of length
    sb16_dsp_write((uint8_t)((length >> 8) & 0xFF)); // High byte of length

    // Wait for the sound to finish playing
    sleep(duration_ms);

    // Turn the speaker off
    sb16_dsp_write(0xD3);
}