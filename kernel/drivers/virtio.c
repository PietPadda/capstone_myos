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

    // Feature negotiation (Modern vs. Legacy)
    // Select the upper 32 bits of the feature flags (for bit 32 and above)
    virtio_sound_cfg->device_feature_select = 1;
    uint32_t features_high = virtio_sound_cfg->device_feature;

    // Check if the device offers the VIRTIO_F_VERSION_1 feature (bit 32)
    if (features_high & (1 << (VIRTIO_F_VERSION_1 - 32))) {
        // Modern Device Path        
        print_string("  Device is VIRTIO_F_VERSION_1 compliant.\n");

         // Acknowledge this one feature
        virtio_sound_cfg->driver_feature_select = 1;
        virtio_sound_cfg->driver_feature = (1 << (VIRTIO_F_VERSION_1 - 32));

        // Set the FEATURES_OK status bit.
        virtio_sound_cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
        print_string("  Status set to FEATURES_OK.\n");

        // Re-read device status to ensure FEATURES_OK is still set.
        if (!(virtio_sound_cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
            print_string("  ERROR: Device rejected features!\n");
            virtio_sound_cfg->device_status |= VIRTIO_STATUS_FAILED;
            return;
        }
    } else {
        // Legacy Device Path

        print_string("  Device is legacy. Omitting FEATURES_OK.\n");
        // For legacy, just write back 0 to the low 32 bits of features.
        virtio_sound_cfg->driver_feature_select = 0;
        virtio_sound_cfg->driver_feature = 0;
    }

    // Virtqueue Setup (Step 7 of the spec)
    print_string("  Setting up virtqueues...\n");

    // Read how many queues the device has.
    uint16_t num_queues = virtio_sound_cfg->num_queues;
    print_string("    Device has "); print_dec(num_queues); print_string(" queues.\n");

    // Select the first queue (queue 0, the control queue).
    virtio_sound_cfg->queue_select = 0;

    // Read its size.
    uint16_t queue_size = virtio_sound_cfg->queue_size;
    print_string("    Queue 0 size: "); print_dec(queue_size); print_string("\n");
    if (queue_size == 0) {
        print_string("    ERROR: Queue 0 not available!\n");
        return;
    }

    // Allocate memory for the three parts of the virtqueue.
    // For now, a single 4KB page for each is more than enough.
    void* desc_table = pmm_alloc_frame();
    void* avail_ring = pmm_alloc_frame();
    void* used_ring = pmm_alloc_frame();
    print_string("    Virtqueue memory allocated.\n");

    // Tell the device the physical addresses of these memory regions.
    virtio_sound_cfg->queue_desc_low = (uint32_t)desc_table;
    virtio_sound_cfg->queue_desc_high = 0;
    virtio_sound_cfg->queue_avail_low = (uint32_t)avail_ring;
    virtio_sound_cfg->queue_avail_high = 0;
    virtio_sound_cfg->queue_used_low = (uint32_t)used_ring;
    virtio_sound_cfg->queue_used_high = 0;
    print_string("    Device notified of queue addresses.\n");

    // Enable the queue.
    virtio_sound_cfg->queue_enable = 1;
    print_string("    Queue 0 is now enabled.\n");

    // End of Virtqueue Setup

    // Set the DRIVER_OK status bit. Device is now live!
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    print_string("  Status set to DRIVER_OK. Device is live!\n");
}