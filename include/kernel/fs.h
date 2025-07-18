// myos/kernel/fs.h

#ifndef FS_H
#define FS_H

#include <kernel/types.h>

// This struct maps to the FAT12 BIOS Parameter Block (BPB) in the boot sector.
// It contains all the essential metadata about the filesystem.
typedef struct {
    uint8_t  jump_code[3];          // The x86 JMP instruction to the boot code.
    uint8_t  oem_name[8];           // A string identifying who formatted the disk.
    uint16_t bytes_per_sector;      // How many bytes in a sector (usually 512).
    uint8_t  sectors_per_cluster;   // How many sectors are in an allocation unit (a "cluster").
    uint16_t reserved_sectors;      // Number of sectors before the first FAT, including this one.
    uint8_t  num_fats;              // The number of File Allocation Tables (usually 2).
    uint16_t root_dir_entries;      // Maximum number of files in the root directory.
    uint16_t total_sectors;         // Total sectors on the disk (if < 65536).
    uint8_t  media_type;            // An old field describing the disk type.
    uint16_t sectors_per_fat;       // How many sectors each FAT occupies.
    uint16_t sectors_per_track;     // Part of the old CHS geometry.
    uint16_t num_heads;             // Part of the old CHS geometry.
    uint32_t hidden_sectors;        // Sectors before this partition begins.
    uint32_t total_sectors_large;   // Total sectors on the disk (used if total_sectors is 0).
} __attribute__((packed)) fat12_bpb_t;

// This struct defines the exact 32-byte layout of a single directory entry
// as it is stored on a FAT12/16/32 formatted disk.
typedef struct {
    uint8_t  name[8];              // 8-byte filename, padded with spaces.
    uint8_t  extension[3];         // 3-byte extension, padded with spaces.
    uint8_t  attributes;           // Bitmask of file attributes (e.g., Read-Only, Directory).
    uint8_t  reserved;             // Reserved for use by Windows NT.
    uint8_t  creation_time_tenths; // Timestamp for creation time (tenths of a second).
    uint16_t creation_time;        // Time file was created.
    uint16_t creation_date;        // Date file was created.
    uint16_t last_access_date;     // Date of last access.
    uint16_t first_cluster_high;   // High 16 bits of the start cluster (used in FAT32).
    uint16_t last_mod_time;        // Time of last modification.
    uint16_t last_mod_date;        // Date of last modification.
    uint16_t first_cluster_low;    // Low 16 bits of the file's starting cluster number.
    uint32_t file_size;            // The size of the file in bytes.
} __attribute__((packed)) fat_dir_entry_t;

// The main function to initialize the filesystem driver.
void init_fs();

// Finds a file in the root directory by its name.
fat_dir_entry_t* fs_find_file(const char* filename);

// Reads the contents of a file given its directory entry.
void* fs_read_file(fat_dir_entry_t* entry);

#endif // FS_H