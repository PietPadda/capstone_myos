// myos/kernel/debug.c

#include <kernel/debug.h>
#include <kernel/io.h> // for port_byte_out
#include <kernel/types.h>

// Sends a null-terminated string to the QEMU debug console
void qemu_debug_string(const char* str) {
    while (*str) {
        port_byte_out(0xE9, (unsigned char)*str);
        str++;
    }
}

// Sends a hexadecimal value to the QEMU debug console
void qemu_debug_hex(uint32_t n) {
    qemu_debug_string("0x");
    // Loop through each of the 8 hex digits (nibbles)
    for (int i = 28; i >= 0; i -= 4) {
        // Get the 4 bits for this digit
        uint32_t nibble = (n >> i) & 0xF;
        // Convert the number (0-15) to its ASCII character ('0'-'9', 'A'-'F')
        char c = (nibble > 9) ? (nibble - 10 + 'A') : (nibble + '0');
        port_byte_out(0xE9, (unsigned char)c);
    }
}

// Dumps a block of memory to the QEMU debug console
void qemu_debug_memdump(const void* addr, size_t size) {
    const uint8_t* byte_ptr = (const uint8_t*)addr;
    qemu_debug_string("Dumping memory at ");
    qemu_debug_hex((uint32_t)addr);
    qemu_debug_string(" for ");
    qemu_debug_hex(size);
    qemu_debug_string(" bytes...\n");

    for (size_t i = 0; i < size; ++i) {
        // Print the byte as a 2-digit hex value
        uint8_t byte = byte_ptr[i];
        char nibble_high = (byte >> 4) & 0xF;
        char nibble_low = byte & 0xF;
        port_byte_out(0xE9, (nibble_high > 9) ? (nibble_high - 10 + 'A') : (nibble_high + '0'));
        port_byte_out(0xE9, (nibble_low > 9) ? (nibble_low - 10 + 'A') : (nibble_low + '0'));
        port_byte_out(0xE9, ' ');

        // Add a newline every 16 bytes for readability
        if ((i + 1) % 16 == 0) {
            port_byte_out(0xE9, '\n');
        }
    }
    port_byte_out(0xE9, '\n');
}