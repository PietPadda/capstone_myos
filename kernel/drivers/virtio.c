// myos/kernel/drivers/virtio.c

#include <kernel/drivers/virtio.h>
#include <kernel/paging.h>
#include <kernel/vga.h>
#include <kernel/memory.h> // For malloc
#include <kernel/timer.h> // For sleep()

// We will map the device's MMIO registers to this virtual address.
#define VIRTIO_SND_VIRT_ADDR 0xE0000000

// A struct to manage the state of a single virtqueue
typedef struct {
    // Virtual addresses of the queue components
    struct virtq_desc* desc_table;
    struct virtq_avail* avail_ring;
    struct virtq_used* used_ring;
    uint16_t size;
    uint16_t last_used_idx;  // To track what the device has used
    uint16_t next_avail_idx; // To track where we should place the next descriptor
} virtqueue_info_t;

// We will have multiple queues; this array will manage them.
#define VIRTIO_SND_MAX_QUEUES 4
static virtqueue_info_t queues[VIRTIO_SND_MAX_QUEUES];

static virtio_pci_common_cfg_t* virtio_sound_cfg;

// Helper to notify the device that a queue has new buffers.
static void notify_queue(uint16_t queue_idx) {
    // The device tells us the memory-mapped offset for its notification register.
    // We write the index of the queue that we've updated.
    volatile uint16_t* notify_reg = (uint16_t*)((uint8_t*)virtio_sound_cfg + virtio_sound_cfg->queue_notify_off);
    *notify_reg = queue_idx;
}

// Submits a command and waits for a reply. This is a synchronous, blocking function.
static void virtq_send_command_sync(uint16_t q_idx, void* cmd, uint32_t cmd_size, void* resp, uint32_t resp_size) {
    virtqueue_info_t* q = &queues[q_idx];

    // We need two descriptors: one for the command we send, one for the response we receive.
    uint16_t head_idx = q->next_avail_idx;
    uint16_t resp_idx = (head_idx + 1) % q->size;
    
    // Setup Descriptor 1: The Command (Driver -> Device)
    q->desc_table[head_idx].addr = (uint64_t)cmd;
    q->desc_table[head_idx].len = cmd_size;
    q->desc_table[head_idx].flags = VIRTQ_DESC_F_NEXT; // Link to the response buffer descriptor
    q->desc_table[head_idx].next = resp_idx;

    // Setup Descriptor 2: The Response (Device -> Driver)
    q->desc_table[resp_idx].addr = (uint64_t)resp;
    q->desc_table[resp_idx].len = resp_size;
    q->desc_table[resp_idx].flags = VIRTQ_DESC_F_WRITE; // Device will WRITE to this buffer
    q->desc_table[resp_idx].next = 0;

    // Make the descriptor chain available to the device
    q->avail_ring->ring[q->avail_ring->idx % q->size] = head_idx;
    __asm__ __volatile__ ("" : : : "memory"); // Memory barrier
    q->avail_ring->idx++; // This makes it visible
    __asm__ __volatile__ ("" : : : "memory");

    // Update our internal counter for the next free descriptor.
    q->next_avail_idx = (resp_idx + 1) % q->size;

    notify_queue(q_idx);

    // Wait for the device to finish
    while (q->last_used_idx == q->used_ring->idx) {
        // In a real driver, we'd use interrupts. For now, polling with a small
        // delay is simple and effective.
        sleep(1); 
    }
    // "Consume" the used ring entry by incrementing our counter.
    q->last_used_idx++;
}

// Initializes the virtio-sound driver.
void virtio_sound_init(virtio_pci_common_cfg_t* cfg) {
    print_string("Initializing virtio-sound driver...\n");

    // The mapping is now done by the PCI driver. We just receive and use the pointer.
    virtio_sound_cfg = cfg;

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

    // QUEUE 0 (CONTROL)
    // Select the first queue (queue 0, the control queue).
    virtio_sound_cfg->queue_select = 0;

    // Read its size.
    uint16_t q0_size = virtio_sound_cfg->queue_size;
    print_string("    Queue 0 (Control) size: "); print_dec(q0_size); print_string("\n");

    // err handling
    if (q0_size == 0) {
        print_string("    ERROR: Queue 0 (Control) not available!\n");
        return;
    }

    // Allocate and store pointers for Queue 0
    // Allocate memory for the three parts of the virtqueue.
    // For now, a single 4KB page for each is more than enough.
    void* q0_desc  = pmm_alloc_frame();
    void* q0_avail  = pmm_alloc_frame();
    void* q0_used  = pmm_alloc_frame();
    print_string("    Virtqueue memory allocated.\n");

    // Tell the device the physical addresses of these memory regions.
    queues[0].desc_table = (struct virtq_desc*)q0_desc;
    queues[0].avail_ring = (struct virtq_avail*)q0_avail;
    queues[0].used_ring  = (struct virtq_used*)q0_used;
    queues[0].size = q0_size;
    queues[0].last_used_idx = 0;
    queues[0].next_avail_idx = 0;
    
    virtio_sound_cfg->queue_desc_low = (uint32_t)q0_desc;
    virtio_sound_cfg->queue_avail_low = (uint32_t)q0_avail;
    virtio_sound_cfg->queue_used_low = (uint32_t)q0_used;
    print_string("    Device notified of queue addresses.\n");

    // Enable the queue.
    virtio_sound_cfg->queue_enable = 1;
    print_string("    Queue 0 (Control) is now enabled.\n");

    // QUEUE 2 (PLAYBACK/TX)
    virtio_sound_cfg->queue_select = 2;
    uint16_t q2_size = virtio_sound_cfg->queue_size;
    print_string("    Queue 2 (Playback) size: "); print_dec(q2_size); print_string("\n");

    // Allocate and store pointers for Queue 2
    void* q2_desc = pmm_alloc_frame();
    void* q2_avail = pmm_alloc_frame();
    void* q2_used = pmm_alloc_frame();
    queues[2].desc_table = (struct virtq_desc*)q2_desc;
    queues[2].avail_ring = (struct virtq_avail*)q2_avail;
    queues[2].used_ring  = (struct virtq_used*)q2_used;
    queues[2].size = q2_size;
    queues[2].last_used_idx = 0;
    queues[2].next_avail_idx = 0;

    virtio_sound_cfg->queue_desc_low = (uint32_t)q2_desc;
    virtio_sound_cfg->queue_avail_low = (uint32_t)q2_avail;
    virtio_sound_cfg->queue_used_low = (uint32_t)q2_used;
    virtio_sound_cfg->queue_enable = 1;
    print_string("    Queue 2 (Playback) is now enabled.\n");

    // End of Virtqueue Setup

    // Set the DRIVER_OK status bit. Device is now live!
    virtio_sound_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    print_string("  Status set to DRIVER_OK. Device is live!\n");
}