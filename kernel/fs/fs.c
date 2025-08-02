// myos/kernel/fs/fs.c

#include <kernel/fs.h>
#include <kernel/disk.h>
#include <kernel/vga.h>
#include <kernel/memory.h>
#include <kernel/io.h>
#include <kernel/string.h> // For our new string functions
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <kernel/debug.h>

fat12_bpb_t* bpb; // make global
static uint8_t* fat_buffer;
static uint32_t data_area_start_sector;
uint8_t* root_directory_buffer; // global ie no static
uint32_t root_directory_size; // global ie no static

// This label is defined in dma_buffer.asm.
extern uint8_t dma_buffer[];

void init_fs() {
    // Read the BIOS Parameter Block (Sector 0) into a temporary buffer.
    uint8_t* temp_buffer = (uint8_t*)pmm_alloc_frame();
    read_disk_sector(0, temp_buffer);

    // Allocate a permanent, correctly-sized buffer for the BPB on the heap.
    bpb = (fat12_bpb_t*)malloc(sizeof(fat12_bpb_t));

    // Copy the BPB data from the temporary sector buffer to its new home.
    memcpy(bpb, temp_buffer, sizeof(fat12_bpb_t));

    // Now that we've copied the data, we can safely free the temporary buffer.
    pmm_free_frame(temp_buffer);

    // --- FAT BUFFER SETUP ---
    // Calculate the size and number of pages needed for the FAT.
    uint32_t fat_size_bytes = bpb->sectors_per_fat * bpb->bytes_per_sector;
    uint32_t fat_pages_needed = (fat_size_bytes + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    // Define a virtual start address for our FAT buffer. Let's use 3MB,
    // which is safely within our initial 4MB identity map.
    uint32_t fat_virt_addr = 0x300000;
    fat_buffer = (uint8_t*)fat_virt_addr;

    // Allocate physical pages and map them to our contiguous virtual space.
    for (uint32_t i = 0; i < fat_pages_needed; i++) {
        void* phys_frame = pmm_alloc_frame();
        paging_map_page(kernel_directory, fat_virt_addr + (i * PMM_FRAME_SIZE), (uint32_t)phys_frame, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
    }
    
    // Now we can safely read the entire FAT into the virtual buffer.
    for (uint32_t i = 0; i < bpb->sectors_per_fat; i++) {
        read_disk_sector(bpb->reserved_sectors + i, fat_buffer + (i * bpb->bytes_per_sector));
    }

    // Verify initial mapping
    qemu_debug_string("INIT_FS: Post-map check of fat_buffer:\n");
    //paging_dump_entry_for_addr((uint32_t)fat_buffer);

    // --- ROOT DIRECTORY BUFFER SETUP ---
    uint32_t root_dir_start_sector = bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
    root_directory_size = (bpb->root_dir_entries * sizeof(fat_dir_entry_t));
    uint32_t root_dir_sectors = (root_directory_size + bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;

    // Define a virtual start address for the root directory buffer, after the FAT.
    uint32_t root_dir_virt_addr = fat_virt_addr + (fat_pages_needed * PMM_FRAME_SIZE);
    root_directory_buffer = (uint8_t*)root_dir_virt_addr;

    // Allocate and map pages for the root directory.
    for (uint32_t i = 0; i < (root_dir_sectors * 512 + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE; i++) {
        void* phys_frame = pmm_alloc_frame();
        paging_map_page(kernel_directory, root_dir_virt_addr + (i * PMM_FRAME_SIZE), (uint32_t)phys_frame, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
    }

    // Read the root directory into the new virtual buffer.
    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        read_disk_sector(root_dir_start_sector + i, root_directory_buffer + (i * 512));
    }

    // --- CLEANUP ---
    data_area_start_sector = root_dir_start_sector + root_dir_sectors;
}

// Reads a 12-bit FAT entry from the in-memory FAT buffer.
uint16_t fs_get_fat_entry(uint16_t cluster) {
    // Each FAT12 entry is 1.5 bytes, so we multiply by 1.5 (or 3/2) to get the byte offset.
    uint32_t fat_offset = (cluster * 3) / 2;

    // Verify mapping at time of use
    qemu_debug_string("FS_GET_FAT_ENTRY: Pre-access check:\n");
    //paging_dump_entry_for_addr((uint32_t)fat_buffer + fat_offset);

    uint32_t fat_size_bytes = bpb->sectors_per_fat * bpb->bytes_per_sector;

    // --- Defensive Bounds Check ---
    // We check for fat_offset + 1 because we need to read two bytes.
    if (fat_offset + 1 >= fat_size_bytes) {
        qemu_debug_string("  FAT_GET: offset is out of bounds! Returning EOC.\n");
        return 0xFFF; // Return End-of-Chain
    }

    // Log the inputs to the function.
    qemu_debug_string("  FAT_GET: cluster=");
    qemu_debug_hex(cluster);
    qemu_debug_string(", calculated_offset=");
    qemu_debug_hex(fat_offset);
    qemu_debug_string("\n");

    // --- Safe, Byte-by-Byte Read to Avoid Unaligned Access ---
    uint8_t byte1 = fat_buffer[fat_offset];
    uint8_t byte2 = fat_buffer[fat_offset + 1];
    uint16_t entry = (byte2 << 8) | byte1; // Combine them into a 16-bit value

    // Log the raw 16-bit value we read from the FAT.
    qemu_debug_string("  FAT_GET: raw_entry_read=0x");
    qemu_debug_hex(entry);
    qemu_debug_string("\n");
    
    // The logic depends on whether the cluster number is even or odd.
    if (cluster % 2 == 0) {
        // For an even cluster, we want the lower 12 bits.
        entry &= 0x0FFF;
    } else {
        // For an odd cluster, we want the upper 12 bits.
        entry >>= 4;
    }

    // Log the final, processed value we are returning.
    qemu_debug_string("  FAT_GET: returning_next_cluster=0x");
    qemu_debug_hex(entry);
    qemu_debug_string("\n");

    return entry;
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
    qemu_debug_string("FS: Entered fs_read_file.\n");
    uint32_t size = entry->file_size;
    qemu_debug_string("FS: File size is ");
    qemu_debug_hex(size);
    qemu_debug_string(" bytes.\n");

    // Handle empty files
    if (size == 0) {
        qemu_debug_string("FS: File is empty, returning dummy buffer.\n");
        return malloc(1); 
    }

    // Allocate a single, contiguous buffer from the kernel's heap.
    uint8_t* file_buffer = (uint8_t*)malloc(size);
    qemu_debug_string("FS: malloc requested, file_buffer is at: ");
    qemu_debug_hex((uint32_t)file_buffer);
    qemu_debug_string("\n");

    // error check
    if (!file_buffer) {
        qemu_debug_string("FS: malloc FAILED.\n");
        return NULL; // Out of memory
    }

    uint8_t* current_pos = file_buffer;
    uint16_t current_cluster = entry->first_cluster_low;
    uint32_t bytes_per_cluster = bpb->sectors_per_cluster * bpb->bytes_per_sector;

    // This loop is now safe because our buffer is large enough.
    while (current_cluster < 0xFF8) { // 0xFF8 is the End-of-Chain marker for FAT12
        qemu_debug_string("FS: Reading cluster #");
        qemu_debug_hex(current_cluster);
        qemu_debug_string("...\n");

        // Read a cluster into our safe, static DMA buffer.
        qemu_debug_string("  -> Calling read_disk_sector...\n");
        read_disk_sector(data_area_start_sector + (current_cluster - 2), dma_buffer);
        qemu_debug_string("  -> Returned from read_disk_sector.\n");

        // Figure out how much of this cluster to copy.
        uint32_t remaining = size - (current_pos - file_buffer);
        uint32_t to_copy = (remaining > bytes_per_cluster) ? bytes_per_cluster : remaining;

        // Copy the data from the dma buffer to our main file buffer.
        qemu_debug_string("  -> Calling memcpy to dest: ");
        qemu_debug_hex((uint32_t)current_pos);
        qemu_debug_string(" for ");
        qemu_debug_hex(to_copy);
        qemu_debug_string(" bytes.\n");
        
         // Use our new debug version of memcpy
        memcpy_debug(current_pos, dma_buffer, to_copy);
        // memcpy(current_pos, dma_buffer, to_copy);

        qemu_debug_string("  -> Returned from memcpy.\n");

        current_pos += to_copy;
        current_cluster = fs_get_fat_entry(current_cluster);
    }

    qemu_debug_string("FS: Finished reading all clusters.\n");
    return file_buffer;
}