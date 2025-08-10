// myos/kernel/drivers/virtio.c

#include <kernel/drivers/virtio.h>
#include <kernel/paging.h>
#include <kernel/vga.h>

// We will map the device's MMIO registers to this virtual address.
#define VIRTIO_SND_VIRT_ADDR 0xE0000000

static virtio_pci_common_cfg_t* virtio_sound_cfg;

// Initializes the virtio-sound driver.
void virtio_sound_init(uint32_t mmio_base_phys) {
    print_string("Initializing virtio-sound driver...\n");

    // Map the physical MMIO base address to our chosen virtual address.
    paging_map_page(kernel_directory, VIRTIO_SND_VIRT_ADDR, mmio_base_phys, PAGING_FLAG_PRESENT | PAGING_FLAG_RW);
    print_string("  MMIO region mapped to virtual address 0x");
    print_hex(VIRTIO_SND_VIRT_ADDR);
    print_string("\n");

    // Create a pointer to the registers using our struct.
    virtio_sound_cfg = (virtio_pci_common_cfg_t*)VIRTIO_SND_VIRT_ADDR;

    // Test read: Let's read the device status register.
    // A virtio device starts in a state of 0.
    uint8_t status = virtio_sound_cfg->device_status;
    print_string("  Initial device status: 0x");
    print_hex(status);
    print_string("\n");
}