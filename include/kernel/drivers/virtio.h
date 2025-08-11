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

// Virtio PCI Capability IDs (from Virtio Spec 4.1.3)
#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

// The structure of a Virtio PCI capability
typedef struct {
    uint8_t cap_vndr;    // Should be 0x09 for PCI standard capabilities
    uint8_t cap_next;    // Offset of the next capability in the list
    uint8_t cap_len;     // Length of this capability structure
    uint8_t type;        // Type of virtio capability (VIRTIO_PCI_CAP_*)
    uint8_t bar;         // Which BAR the capability is in
    uint8_t padding[3];
    uint32_t offset;     // Offset within the BAR
    uint32_t length;     // Length of the structure in the BAR
} __attribute__((packed)) virtio_pci_cap_t;

// Virtio Sound Command Codes
#define VIRTIO_SND_R_PCM_SET_PARAMS  0x0101
#define VIRTIO_SND_R_PCM_PREPARE     0x0102
#define VIRTIO_SND_R_PCM_RELEASE     0x0103
#define VIRTIO_SND_R_PCM_START       0x0104
#define VIRTIO_SND_R_PCM_STOP        0x0105

// Generic command header for the control queue
typedef struct {
    uint32_t code;
} __attribute__((packed)) virtio_snd_hdr_t;

// Command structure for preparing, starting, or stopping a PCM stream
typedef struct {
    virtio_snd_hdr_t hdr;
    uint32_t stream_id;
} __attribute__((packed)) virtio_snd_pcm_hdr_t;

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

// Virtqueue Descriptor Flags
#define VIRTQ_DESC_F_NEXT  1 // This descriptor is part of a chain
#define VIRTQ_DESC_F_WRITE 2 // The device will write to this buffer (as opposed to read from it)

// Virtio Sound Sample Formats and Rates
#define VIRTIO_SND_PCM_FMT_U8       2  // Unsigned 8-bit PCM
#define VIRTIO_SND_PCM_RATE_44100   11 // 44100 Hz

// Command structure for setting PCM stream parameters.
typedef struct {
    virtio_snd_pcm_hdr_t hdr;
    uint32_t buffer_bytes;
    uint32_t period_bytes;
    uint32_t features;
    uint8_t  channels;
    uint8_t  format;
    uint8_t  rate;
    uint8_t  padding;
} __attribute__((packed)) virtio_snd_pcm_set_params_t;

// A Virtqueue Descriptor, which points to a buffer of data.
struct virtq_desc {
    uint64_t addr;  // Physical address of the buffer
    uint32_t len;   // Length of the buffer
    uint16_t flags; // Flags (e.g., VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE)
    uint16_t next;  // Index of the next descriptor in a chain
} __attribute__((packed));

// The "Available" Ring, used by the driver to offer buffers to the device.
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[]; // Array of descriptor indices
} __attribute__((packed));

// An entry in the "Used" Ring, used by the device to return buffers to the driver.
struct virtq_used_elem {
    uint32_t id;  // Index of the used descriptor chain head
    uint32_t len; // Total bytes written to the buffer
} __attribute__((packed));

// The "Used" Ring.
struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
} __attribute__((packed));

// Initializes the virtio-sound driver.
// The signature is changed to take a pointer to the already-mapped config.
// The signature is changed to accept the notification multiplier.
void virtio_sound_init(virtio_pci_common_cfg_t* cfg, uint32_t notify_multiplier);

// Plays a test beep using the virtio-sound device.
void virtio_sound_beep();

// Probes the device to read and display its configuration registers.
void virtio_sound_probe();

#endif // VIRTIO_H