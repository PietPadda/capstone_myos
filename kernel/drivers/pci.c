// myos/kernel/drivers/pci.c

#include <kernel/drivers/pci.h>
#include <kernel/io.h>
#include <kernel/vga.h>

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

// Scans the PCI bus for our virtio-sound device.
void pci_scan() {
    print_string("Scanning PCI bus...\n");

    // Iterate through all possible buses, devices, and functions.
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                // Read the first 32 bits of the configuration space, which contains Vendor and Device ID.
                uint32_t first_dword = pci_config_read_word(bus, slot, func, 0);
                uint16_t vendor_id = first_dword & 0xFFFF;
                
                // If vendor ID is 0xFFFF, no device is present.
                if (vendor_id == 0xFFFF) {
                    continue;
                }

                uint16_t device_id = first_dword >> 16;

                // Print info for every device we find.
                print_string("  Found device: bus="); print_dec(bus);
                print_string(", slot="); print_dec(slot);
                print_string(", func="); print_dec(func);
                print_string(" | VID=0x"); print_hex(vendor_id);
                print_string(", DID=0x"); print_hex(device_id);
                print_string("\n");
            }
        }
    }
}