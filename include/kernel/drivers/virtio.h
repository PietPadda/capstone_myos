// myos/include/kernel/drivers/virtio.h

#ifndef VIRTIO_H
#define VIRTIO_H

#include <kernel/types.h>

// Virtio Device Status Field bits
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

// Virtio Feature Bits
#define VIRTIO_F_VERSION_1 32 // The modern device specification feature bit

// Represents the "Common Configuration" registers for a virtio PCI device.
// The 'volatile' keyword is crucial to prevent the compiler from optimizing
// away our reads and writes to this memory-mapped hardware.
typedef struct {
    volatile uint32_t device_feature_select;
    volatile uint32_t device_feature;
    volatile uint32_t driver_feature_select;
    volatile uint32_t driver_feature;
    volatile uint16_t msix_config;
    volatile uint16_t num_queues;
    volatile uint8_t  device_status;
    volatile uint8_t  config_generation;
    // --- Queue Configuration ---
    volatile uint16_t queue_select;
    volatile uint16_t queue_size;
    volatile uint16_t queue_msix_vector;
    volatile uint16_t queue_enable;
    volatile uint16_t queue_notify_off;
    volatile uint32_t queue_desc_low;
    volatile uint32_t queue_desc_high;
    volatile uint32_t queue_avail_low;
    volatile uint32_t queue_avail_high;
    volatile uint32_t queue_used_low;
    volatile uint32_t queue_used_high;
} __attribute__((packed)) virtio_pci_common_cfg_t;

// Initializes the virtio-sound driver.
void virtio_sound_init(uint32_t mmio_base);

#endif // VIRTIO_H