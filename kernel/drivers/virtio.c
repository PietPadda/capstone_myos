// myos/kernel/drivers/virtio.c

#include <kernel/drivers/virtio.h>
#include <kernel/paging.h>
#include <kernel/vga.h>
#include <kernel/memory.h> // For malloc
#include <kernel/timer.h> // For sleep()
#include <kernel/string.h> // for memcpy

// Forward-declare our new static function before it's used.
static void virtq_send_buffer(uint16_t q_idx, void* data, uint32_t len);

// We will map the device's MMIO registers to this virtual address.
#define VIRTIO_SND_VIRT_ADDR 0xE0000000

// We will have multiple queues; this array will manage them.
#define VIRTIO_SND_MAX_QUEUES 4

// global for the notification base address.
static volatile void* notify_base_addr;
static uint32_t notify_off_multiplier;

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

static virtqueue_info_t queues[VIRTIO_SND_MAX_QUEUES];
static virtio_pci_common_cfg_t* virtio_sound_cfg;
static uint32_t notify_off_multiplier; // Global to store the multiplier

// A static, page-aligned buffer for DMA. Its virtual address will equal its physical address.
static uint8_t dma_buffer[4096] __attribute__((aligned(4096)));

// Helper to notify the device that a queue has new buffers.
// Updated to use the multiplier in its calculation.
static void notify_queue(uint16_t queue_idx) {
    // The device tells us the memory-mapped offset for its notification register.
    // The offset is calculated from the common config struct.
    uint32_t notify_offset = virtio_sound_cfg->queue_notify_off * notify_off_multiplier;
    // But the base address is the separate notification region we just mapped.
    volatile uint16_t* notify_reg = (uint16_t*)((uint8_t*)notify_base_addr + notify_offset);
    *notify_reg = queue_idx;
}

// Submits a command and waits for a reply. This is a synchronous, blocking function.
// Updated to use the DMA-safe buffer for all communication.
static void virtq_send_command_sync(uint16_t q_idx, void* cmd, uint32_t cmd_size, void* resp, uint32_t resp_size) {
    // Copy the command from its virtual stack address into our safe physical buffer.
    memcpy(dma_buffer, cmd, cmd_size);
    // The device will write the response right after the command data.
    void* dma_resp_phys_addr = dma_buffer + cmd_size;

    virtqueue_info_t* q = &queues[q_idx];

    // We need two descriptors: one for the command we send, one for the response we receive.
    uint16_t head_idx = q->next_avail_idx;
    uint16_t resp_idx = (head_idx + 1) % q->size;
    
    // Setup Descriptor 1: The Command (Driver -> Device)
    q->desc_table[head_idx].addr = (uint64_t)dma_buffer; // Use the PHYSICAL address of our DMA buffer.
    q->desc_table[head_idx].len = cmd_size;
    q->desc_table[head_idx].flags = VIRTQ_DESC_F_NEXT; // Link to the response buffer descriptor
    q->desc_table[head_idx].next = resp_idx;

    // Setup Descriptor 2: The Response (Device -> Driver)
    q->desc_table[resp_idx].addr = (uint64_t)dma_resp_phys_addr; //  Use the PHYSICAL address for the response part of the buffer.
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

    // Copy the response from the safe physical buffer back to the caller's virtual stack address.
    memcpy(resp, dma_resp_phys_addr, resp_size);
}

// Initializes the virtio-sound driver.
// Updated to accept the multiplier.
void virtio_sound_init(virtio_pci_common_cfg_t* cfg, void* notify_base, uint32_t multiplier){
    print_string("Initializing virtio-sound driver...\n");

    // The mapping is now done by the PCI driver. We just receive and use the pointer.
    virtio_sound_cfg = cfg;
    notify_base_addr = notify_base; // Store the notification base virtual address
    notify_off_multiplier = multiplier; // Store the multiplier for later use.

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

// This is a generic response struct for control commands.
// The device writes a status code here (e.g., VIRTIO_SND_S_OK).
typedef struct {
    uint32_t code;
} __attribute__((packed)) virtio_snd_response_t;

// This function orchestrates the lifecycle of a sound stream.
void virtio_sound_beep() {
    // We will use stream 0 for playback.
    uint32_t stream_id = 0;

    // Set stream parameters
    virtio_snd_pcm_set_params_t params_cmd;
    params_cmd.hdr.hdr.code = VIRTIO_SND_R_PCM_SET_PARAMS;
    params_cmd.hdr.stream_id = stream_id;
    params_cmd.buffer_bytes = 4096; // Total size of buffers we'll use
    params_cmd.period_bytes = 2048; // Device can interrupt after this many bytes
    params_cmd.features = 0;        // No special features needed
    params_cmd.channels = 2;        // Request stereo
    params_cmd.format = VIRTIO_SND_PCM_FMT_S16_LE; // 16-bit signed
    params_cmd.rate = VIRTIO_SND_PCM_RATE_44100;
    
    virtio_snd_response_t params_resp;
    print_string("\n  Sending SET_PARAMS command...");
    virtq_send_command_sync(0, &params_cmd, sizeof(params_cmd), &params_resp, sizeof(params_resp));
    print_string(" complete.");

    // Prepare the stream
    virtio_snd_pcm_hdr_t prepare_cmd = { .hdr.code = VIRTIO_SND_R_PCM_PREPARE, .stream_id = stream_id };
    virtio_snd_response_t prepare_resp;
    print_string("\n  Sending PREPARE command...");
    virtq_send_command_sync(0, &prepare_cmd, sizeof(prepare_cmd), &prepare_resp, sizeof(prepare_resp));
    print_string(" complete.");

    // Start the stream
    virtio_snd_pcm_hdr_t start_cmd = { .hdr.code = VIRTIO_SND_R_PCM_START, .stream_id = stream_id };
    virtio_snd_response_t start_resp;
    print_string("\n  Sending START command...");
    virtq_send_command_sync(0, &start_cmd, sizeof(start_cmd), &start_resp, sizeof(start_resp));
    print_string(" complete. Stream is active.");

    // Generate and Send Audio Data
    print_string("\n  Generating and sending audio buffer...");
    // Cast our buffer to signed 16-bit for easier access.
    uint8_t* audio_buffer = (int16_t*)dma_buffer; // Use our existing DMA-safe buffer
    uint32_t buffer_size_bytes  = 2048; // bytes
    // Each sample is 2 bytes, and it's stereo, so we have buffer_size_bytes / 4 audio frames.
    uint32_t frame_count = buffer_size_bytes  / 4;
    uint16_t freq = 440; // A4 note
    uint16_t sample_rate = 44100;
    int16_t amplitude = 8000; // A good volume for 16-bit audio
    
    uint32_t period_frames = sample_rate / freq;
    for (uint32_t i = 0; i < frame_count; i++) {
        // Generate a square wave. Midpoint is 0 for signed audio.
        int16_t sample_value = ((i % period_frames) < (period_frames / 2)) ? amplitude : -amplitude;
        // Write the same sample to both left and right channels.
        audio_buffer[i * 2 + 0] = sample_value; // Left channel
        audio_buffer[i * 2 + 1] = sample_value; // Right channel
    }

    // Send the buffer to the PLAYBACK queue (queue 2).
    virtq_send_buffer(2, audio_buffer, buffer_size_bytes );
    
    // Give it time to play. 2048 bytes at 44.1kHz is ~46ms.
    sleep(100);
    print_string(" complete.");

    // Stop the stream
    virtio_snd_pcm_hdr_t stop_cmd = { .hdr.code = VIRTIO_SND_R_PCM_STOP, .stream_id = stream_id };
    virtio_snd_response_t stop_resp;
    print_string("\n  Sending STOP command...");
    virtq_send_command_sync(0, &stop_cmd, sizeof(stop_cmd), &stop_resp, sizeof(stop_resp));
    print_string(" complete.");

    // Release the stream
    virtio_snd_pcm_hdr_t release_cmd = { .hdr.code = VIRTIO_SND_R_PCM_RELEASE, .stream_id = stream_id };
    virtio_snd_response_t release_resp;
    print_string("\n  Sending RELEASE command...");
    virtq_send_command_sync(0, &release_cmd, sizeof(release_cmd), &release_resp, sizeof(release_resp));
    print_string(" complete. Stream is inactive.\n");
}

// Debug probing critical values
void virtio_sound_probe() {
    // This function assumes virtio_sound_init has already run and set the pointer.
    if (!virtio_sound_cfg) {
        print_string("  ERROR: virtio_sound_cfg is NULL. Was init successful?\n");
        return;
    }

    // Read registers directly from the mapped MMIO structure.
    print_string("  Device Status: 0x");
    print_hex(virtio_sound_cfg->device_status);
    print_string("\n  Number of Queues: ");
    print_dec(virtio_sound_cfg->num_queues);
    print_string("\n  Device Features (Low 32 bits): 0x");
    virtio_sound_cfg->device_feature_select = 0; // Select the low bits
    print_hex(virtio_sound_cfg->device_feature);
    print_string("\n");
}

// Submits a single data buffer to a queue without waiting for a response.
static void virtq_send_buffer(uint16_t q_idx, void* data, uint32_t len) {
    virtqueue_info_t* q = &queues[q_idx];

    uint16_t head_idx = q->next_avail_idx;

    // --- Setup the single descriptor for our data buffer ---
    q->desc_table[head_idx].addr = (uint64_t)(uint32_t)data;
    q->desc_table[head_idx].len = len;
    q->desc_table[head_idx].flags = 0; // No flags needed for a simple read-only buffer
    q->desc_table[head_idx].next = 0;

    // --- Make it available ---
    q->avail_ring->ring[q->avail_ring->idx % q->size] = head_idx;
    __asm__ __volatile__ ("" : : : "memory");
    q->avail_ring->idx++;
    __asm__ __volatile__ ("" : : : "memory");

    // Update our internal index for the next free descriptor
    q->next_avail_idx = (head_idx + 1) % q->size;

    notify_queue(q_idx);
}