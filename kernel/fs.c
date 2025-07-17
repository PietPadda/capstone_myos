// myos/kernel/fs.c

#include "fs.h"
#include "disk.h"
#include "vga.h"
#include "memory.h"
#include "io.h"
#include "string.h" // For our new string functions

static fat12_bpb_t* bpb; // keep private to fs/c
uint8_t* root_directory_buffer; // global ie no static
uint32_t root_directory_size; // global ie no static

void init_fs() {
    // Read the BIOS Parameter Block (Sector 0)
    uint8_t* buffer = (uint8_t*)malloc(512);
    read_disk_sector(0, buffer);
    bpb = (fat12_bpb_t*)buffer;
    port_byte_out(0xE9, '1'); // BPB should be read now

    // Calculate the location and size of the root directory
    uint32_t root_dir_start_sector = bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
    root_directory_size = (bpb->root_dir_entries * sizeof(fat_dir_entry_t));
    uint32_t root_dir_size_sectors = root_directory_size / bpb->bytes_per_sector;
    if (root_directory_size % bpb->bytes_per_sector > 0) {
        root_dir_size_sectors++; // Handle non-even division
    }
    port_byte_out(0xE9, '2'); // Root directory parameters calculated

    // Read the entire root directory into a new buffer
    root_directory_buffer = (uint8_t*)malloc(root_dir_size_sectors * bpb->bytes_per_sector);
    port_byte_out(0xE9, '3'); // About to start reading root dir sectors
    for (uint32_t i = 0; i < root_dir_size_sectors; i++) {
        read_disk_sector(root_dir_start_sector + i, root_directory_buffer + (i * 512));
        port_byte_out(0xE9, '.'); // Print a dot for each sector read
    }
    port_byte_out(0xE9, '4'); // FINISHED filesystem init */
}

fat_dir_entry_t* fs_find_file(const char* filename) {
    char fat_name[12];
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0'; // Not strictly necessary, but good practice

    // Convert filename to FAT 8.3 format
    int i = 0, j = 0;
    while (filename[i] && filename[i] != '.' && j < 8) {
        fat_name[j++] = toupper(filename[i++]);
    }
    if (filename[i] == '.') i++;
    j = 8;
    while (filename[i] && j < 11) {
        fat_name[j++] = toupper(filename[i++]);
    }

    // Search the directory
    for (uint32_t offset = 0; offset < root_directory_size; offset += sizeof(fat_dir_entry_t)) {
        fat_dir_entry_t* entry = (fat_dir_entry_t*)(root_directory_buffer + offset);

        if (entry->name[0] == 0x00) break;
        if ((uint8_t)entry->name[0] == 0xE5) continue;
        if ((entry->attributes & 0x0F) == 0x0F) continue; // LFN
        if (entry->attributes & 0x08) continue;           // Volume Label

        // Instead of one 11-byte comparison, do two separate ones.
        if (memcmp(fat_name, entry->name, 8) == 0 && memcmp(fat_name + 8, entry->extension, 3) == 0) {
            return entry; // We found it!
        }
    }

    return NULL; // File not found
}