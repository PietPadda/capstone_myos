// myos/kernel/disk.h

#ifndef DISK_H
#define DISK_H

#include <kernel/types.h>

// Reads a 512-byte sector from the disk.
// lba: The linear block address of the sector.
// buffer: A pointer to a 512-byte buffer to store the data.
void read_disk_sector(uint32_t lba, uint8_t* buffer);

#endif