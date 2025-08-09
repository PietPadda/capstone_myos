// myos/include/kernel/drivers/sb16.h

#ifndef SB16_H
#define SB16_H

#include <kernel/types.h>

// The base I/O port for the SB16 as emulated by QEMU
#define SB16_BASE_PORT 0x220

// Mixer Port Offsets from the base
#define SB16_MIXER_ADDR     (SB16_BASE_PORT + 0x4) // Write
#define SB16_MIXER_DATA     (SB16_BASE_PORT + 0x5) // Read/Write

// Mixer Registers
#define SB16_MIXER_MASTER_VOL 0x22
#define SB16_MIXER_VOICE_VOL  0x04

// DSP Port Offsets from the base
#define SB16_DSP_RESET      (SB16_BASE_PORT + 0x6) // Write
#define SB16_DSP_READ       (SB16_BASE_PORT + 0xA) // Read
#define SB16_DSP_WRITE      (SB16_BASE_PORT + 0xC) // Write
#define SB16_DSP_READ_STATUS (SB16_BASE_PORT + 0xE) // Read (Bit 7: 1=Ready to read)
#define SB16_DSP_WRITE_STATUS (SB16_BASE_PORT + 0xC) // Read (Bit 7: 1=Ready to write)

// Initializes the Sound Blaster 16 driver.
void sb16_init();

// Plays a square wave of a given frequency and duration.
void sb16_play_sound(uint16_t frequency, uint16_t duration_ms);

#endif // SB16_H