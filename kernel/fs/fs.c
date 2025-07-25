// myos/kernel/fs/fs.c

#include <kernel/fs.h>
#include <kernel/disk.h>
#include <kernel/vga.h>
#include <kernel/memory.h>
#include <kernel/io.h>
#include <kernel/string.h> // For our new string functions

fat12_bpb_t* bpb; // make global
static uint8_t* fat_buffer;
static uint32_t data_area_start_sector;
uint8_t* root_directory_buffer; // global ie no static
uint32_t root_directory_size; // global ie no static

void init_fs() {
    // Read the BIOS Parameter Block (Sector 0)
    uint8_t* buffer = (uint8_t*)malloc(512);
    read_disk_sector(0, buffer);
    bpb = (fat12_bpb_t*)buffer;

    // Read the FAT into memory
    uint32_t fat_size_bytes = bpb->sectors_per_fat * bpb->bytes_per_sector;
    fat_buffer = (uint8_t*)malloc(fat_size_bytes);
    for (uint32_t i = 0; i < bpb->sectors_per_fat; i++) {
        read_disk_sector(bpb->reserved_sectors + i, fat_buffer + (i * bpb->bytes_per_sector));
    }

    // Calculate the location and size of the root directory
    uint32_t root_dir_start_sector = bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
    root_directory_size = (bpb->root_dir_entries * sizeof(fat_dir_entry_t));
    uint32_t root_dir_size_sectors = root_directory_size / bpb->bytes_per_sector;
    if (root_directory_size % bpb->bytes_per_sector > 0) {
        root_dir_size_sectors++;
    }

    // calc starting sector of data area
    data_area_start_sector = root_dir_start_sector + root_dir_size_sectors;

    // Read the entire root directory into a new buffer
    root_directory_buffer = (uint8_t*)malloc(root_dir_size_sectors * bpb->bytes_per_sector);
    for (uint32_t i = 0; i < root_dir_size_sectors; i++) {
        read_disk_sector(root_dir_start_sector + i, root_directory_buffer + (i * 512));
    }
}

uint16_t fs_get_fat_entry(uint16_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2);
    uint16_t* fat16 = (uint16_t*)&fat_buffer[fat_offset];
    
    if (cluster % 2 == 0) {
        return *fat16 & 0x0FFF; // Even cluster
    } else {
        return *fat16 >> 4;     // Odd cluster
    }
}

void fs_read_cluster(uint16_t cluster, uint8_t* buffer) {
    uint32_t lba = data_area_start_sector + (cluster - 2);
    read_disk_sector(lba, buffer);
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

void* fs_read_file(fat_dir_entry_t* entry) {
    uint32_t size = entry->file_size;
    if (size == 0) return malloc(1); // Handle empty files

    // Allocate enough space for the full clusters to prevent a buffer overflow.
    uint32_t bytes_per_cluster = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    uint32_t num_clusters = (size + bytes_per_cluster - 1) / bytes_per_cluster;
    uint8_t* file_buffer = (uint8_t*)malloc(num_clusters * bytes_per_cluster);

    if (!file_buffer) {
        return NULL;
    }

    uint8_t* current_pos = file_buffer;
    uint16_t current_cluster = entry->first_cluster_low;

    // This loop is now safe because our buffer is large enough.
    while (current_cluster < 0xFF8) { // 0xFF8 is the End-of-Chain marker for FAT12
        fs_read_cluster(current_cluster, current_pos);
        current_pos += bytes_per_cluster;
        current_cluster = fs_get_fat_entry(current_cluster);
    }
    return file_buffer;
}