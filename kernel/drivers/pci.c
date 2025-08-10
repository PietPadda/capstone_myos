// myos/kernel/drivers/pci.c

#include <kernel/drivers/pci.h>
#include <kernel/io.h>
#include <kernel/vga.h>
#include <kernel/drivers/virtio.h>
#include <kernel/paging.h>      // Needed for paging_map_page
#include <kernel/debug.h>       // For qemu_debug

// The virtual address where we will map the virtio registers
#define VIRTIO_SND_VIRT_ADDR 0xE0000000

// Define a safe virtual address for temporary mappings.
#define TEMP_VIRTIO_MAP_ADDR 0xFFBFC000

// We'll store the location of our found virtio device here
static uint8_t virtio_sound_bus = 0;
static uint8_t virtio_sound_slot = 0;
static uint8_t virtio_sound_func = 0;

// Reads a 32-bit word from a device's PCI configuration space.
static uint32_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    // Create the 32-bit address packet.
    // Format: | 31 (Enable) | 30-24 (Reserved) | 23-16 (Bus) | 15-11 (Slot) | 10-8 (Func) | 7-2 (Offset) | 1-0 (00) |
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    // Write the address to the CONFIG_ADDRESS port.
    __asm__ __volatile__ ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDR));

    // Read the 32-bit data from the CONFIG_DATA port.
    uint32_t result;
    __asm__ __volatile__ ("inl %1, %0" : "=a"(result) : "Nd"(PCI_CONFIG_DATA));
    
    return result;
}

// Writes a 32-bit word to a device's PCI configuration space.
static void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    // Create the 32-bit address packet.
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    // Write the address to the CONFIG_ADDRESS port.
    __asm__ __volatile__ ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDR));

    // Write the 32-bit data to the CONFIG_DATA port.
    __asm__ __volatile__ ("outl %0, %1" : : "a"(value), "Nd"(PCI_CONFIG_DATA));
}

// Helper to read a single byte from PCI config space
static uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t word = pci_config_read_word(bus, slot, func, offset & 0xFC);
    return (uint8_t)((word >> ((offset & 3) * 8)) & 0xFF);
}

// Helper to find a specific vendor capability
static bool pci_find_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_id, virtio_pci_cap_t* cap_out) {
    uint8_t cap_ptr = pci_config_read_byte(bus, slot, func, 0x34);
    while (cap_ptr != 0) {
        uint8_t current_cap_id = pci_config_read_byte(bus, slot, func, cap_ptr);
        if (current_cap_id == 0x09) { // Vendor-specific capability ID
            // Read the full capability structure to check the virtio type
            cap_out->type = pci_config_read_byte(bus, slot, func, cap_ptr + 3);
            if (cap_out->type == cap_id) {
                // Found it! Read the rest of the struct.
                cap_out->bar = pci_config_read_byte(bus, slot, func, cap_ptr + 4);
                cap_out->offset = pci_config_read_word(bus, slot, func, cap_ptr + 8);
                cap_out->length = pci_config_read_word(bus, slot, func, cap_ptr + 12);
                return true;
            }
        }
        cap_ptr = pci_config_read_byte(bus, slot, func, cap_ptr + 1); // Get next pointer
    }
    return false;
}

// Scans the PCI bus for our virtio-sound device.
void pci_scan() {
    print_string("Scanning PCI bus...\n");
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t first_dword = pci_config_read_word(bus, slot, 0, 0);
            if ((first_dword & 0xFFFF) == VIRTIO_VENDOR_ID && (first_dword >> 16) == VIRTIO_DEV_ID_SOUND) {
                print_string("  Found virtio-sound device!\n");
                
                virtio_pci_cap_t common_cap;
                if (pci_find_capability(bus, slot, 0, VIRTIO_PCI_CAP_COMMON_CFG, &common_cap)) {
                    print_string("    Found Common Config capability.\n");
                    // Read the base address from the correct BAR
                    uint32_t bar_val = pci_config_read_word(bus, slot, 0, 0x10 + (common_cap.bar * 4));
                    uint32_t bar_base = bar_val & ~0xF;
                    uint32_t phys_addr = bar_base + common_cap.offset;

                    print_string("    Mapping registers at physical 0x"); print_hex(phys_addr); print_string("\n");
                    paging_map_page(kernel_directory, VIRTIO_SND_VIRT_ADDR, phys_addr, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
                    virtio_pci_common_cfg_t* cfg = (virtio_pci_common_cfg_t*)VIRTIO_SND_VIRT_ADDR;

                    // Now, find the notification capability to get the multiplier.
                    virtio_pci_cap_t notify_cap;
                    if (pci_find_capability(bus, slot, 0, VIRTIO_PCI_CAP_NOTIFY_CFG, &notify_cap)) {
                        uint32_t notify_bar_val = pci_config_read_word(bus, slot, 0, 0x10 + (notify_cap.bar * 4));
                        uint32_t notify_phys_addr = (notify_bar_val & ~0xF) + notify_cap.offset;

                        // Temporarily map the notification BAR to read the multiplier.
                        paging_map_page(kernel_directory, TEMP_VIRTIO_MAP_ADDR, notify_phys_addr & ~0xFFF, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
                        uint32_t multiplier = *(volatile uint32_t*)(TEMP_VIRTIO_MAP_ADDR + (notify_phys_addr & 0xFFF));
                        paging_map_page(kernel_directory, TEMP_VIRTIO_MAP_ADDR, 0, 0); // Unmap

                        // Pass both the config pointer and the multiplier to the driver.
                        virtio_sound_init(cfg, multiplier);
                    } else {
                        print_string("    ERROR: Could not find Notification capability!\n");
                    }
                } else {
                    print_string("    ERROR: Could not find Common Config capability!\n");
                }
                return; // Stop scanning
            }
        }
    }
    print_string("  virtio-sound device not found.\n");
}