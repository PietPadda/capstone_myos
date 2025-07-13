// myos/kernel/fs.c

#include "fs.h"
#include "disk.h"
#include "vga.h"
#include "memory.h"

static fat12_bpb_t* bpb; // keep private to fs/c
uint8_t* root_directory_buffer; // global ie no static
uint32_t root_directory_size; // global ie no static

void init_fs() {
    // Read the BIOS Parameter Block (Sector 0)
    uint8_t* buffer = (uint8_t*)malloc(512);
    read_disk_sector(0, buffer);
    bpb = (fat12_bpb_t*)buffer;

    // Calculate the location and size of the root directory
    uint32_t root_dir_start_sector = bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
    root_directory_size = (bpb->root_dir_entries * sizeof(fat_dir_entry_t));
    uint32_t root_dir_size_sectors = root_directory_size / bpb->bytes_per_sector;
    if (root_directory_size % bpb->bytes_per_sector > 0) {
        root_dir_size_sectors++; // Handle non-even division
    }

    // Read the entire root directory into a new buffer
    root_directory_buffer = (uint8_t*)malloc(root_dir_size_sectors * bpb->bytes_per_sector);
    for (uint32_t i = 0; i < root_dir_size_sectors; i++) {
        read_disk_sector(root_dir_start_sector + i, root_directory_buffer + (i * 512));
    }
}