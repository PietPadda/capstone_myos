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

    // Virtio Initialization Sequence
    // Reset the device by writing 0 to the status register.
    virtio_sound_cfg->device_status = 0;
    print_string("  Device reset.\n");

    // Set the ACKNOWLEDGE status bit.
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_ACKNOWLEDGE;
    print_string("  Status set to ACKNOWLEDGE.\n");

    // Set the DRIVER status bit.
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_DRIVER;
    print_string("  Status set to DRIVER.\n");

    // Feature negotiation (for now, we accept all features).
    uint32_t features = virtio_sound_cfg->device_feature;
    virtio_sound_cfg->driver_feature = features;
    print_string("  Features negotiated.\n");

    // Set the FEATURES_OK status bit.
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    print_string("  Status set to FEATURES_OK.\n");

    // Re-read device status to ensure FEATURES_OK is still set.
    if (!(virtio_sound_cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        print_string("  ERROR: Device rejected features!\n");
        virtio_sound_cfg->device_status |= VIRTIO_STATUS_FAILED;
        return;
    }

    // Set the DRIVER_OK status bit. Device is now live!
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    print_string("  Status set to DRIVER_OK. Device is live!\n");
}