// myos/kernel/drivers/pci.c

#include <kernel/drivers/pci.h>
#include <kernel/io.h>
#include <kernel/vga.h>

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

// Scans the PCI bus for our virtio-sound device.
void pci_scan() {
    print_string("Scanning PCI bus...\n");

    // Iterate through all possible buses, devices, and functions.
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            // We only need to check func=0 for the virtio-sound device
            uint32_t first_dword = pci_config_read_word(bus, slot, 0, 0);
            uint16_t vendor_id = first_dword & 0xFFFF;
            uint16_t device_id = first_dword >> 16;

            // Check if we found our virtio-sound device.
            if (vendor_id == VIRTIO_VENDOR_ID && device_id == VIRTIO_DEV_ID_SOUND) {
                print_string("  Found virtio-sound device!\n");
                print_string("    Location: bus="); print_dec(bus);
                print_string(", slot="); print_dec(slot);
                print_string(", func=0\n");

                // Enable Bus Mastering and Memory Space
                // Read the command register (offset 0x04)
                uint32_t command_reg = pci_config_read_word(bus, slot, 0, 0x04);
                // Set the Bus Master Enable (bit 2) and Memory Space Enable (bit 1) bits
                command_reg |= (1 << 2) | (1 << 1);
                // Write the new value back to the command register to enable the device
                pci_config_write_word(bus, slot, 0, 0x04, command_reg);
                print_string("    Device enabled (Bus Master, Memory Space).\n");

                // Iterate through all 6 BARs
                for (int bar_num = 0; bar_num < 6; bar_num++) {
                    uint8_t bar_offset = 0x10 + (bar_num * 4);
                    uint32_t bar_value = pci_config_read_word(bus, slot, 0, bar_offset);

                    if (bar_value == 0) continue; // Skip unused BARs

                    print_string("    BAR"); print_dec(bar_num); print_string(": ");

                    if (bar_value & 0x1) {
                        // Type 1: I/O Space
                        uint32_t io_base = bar_value & ~0x3;
                        print_string("I/O Port Base = 0x"); print_hex(io_base);
                    } else {
                        // Type 0: Memory-Mapped I/O
                        uint8_t mem_type = (bar_value >> 1) & 0x3;
                        if (mem_type == 0x00) { // 32-bit mapping
                            uint32_t mem_base = bar_value & ~0xF;
                            print_string("32-bit MMIO Base = 0x"); print_hex(mem_base);
                        } else if (mem_type == 0x02) { // 64-bit mapping
                            // For 64-bit, this BAR holds the lower 32 bits.
                            // The next BAR holds the upper 32 bits.
                            uint32_t mem_base_low = bar_value & ~0xF;
                            uint32_t mem_base_high = pci_config_read_word(bus, slot, 0, bar_offset + 4);
                            print_string("64-bit MMIO Base = 0x");
                            print_hex(mem_base_high);
                            print_hex(mem_base_low);
                            bar_num++; // IMPORTANT: Skip the next BAR since we just used it
                        }
                    }
                    print_string("\n");
                }
                return; // Found our device, stop scanning
            }
        }
    }
    print_string("  virtio-sound device not found.\n");
}