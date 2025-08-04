// myos/kernel/debug.c

#include <kernel/debug.h>
#include <kernel/io.h> // for port_byte_out
#include <kernel/types.h>
#include <kernel/exceptions.h>  // for registers_t
#include <kernel/cpu/process.h> // for cpu_state_t

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

// Dumps the state of all CPU registers from a registers_t struct.
void qemu_debug_regs(registers_t *r) {
    qemu_debug_string("-- REGISTER DUMP --\n");
    qemu_debug_string("EAX: "); qemu_debug_hex(r->eax); qemu_debug_string("  EBX: "); qemu_debug_hex(r->ebx);
    qemu_debug_string("  ECX: "); qemu_debug_hex(r->ecx); qemu_debug_string("\nEDX: "); qemu_debug_hex(r->edx);
    qemu_debug_string("  ESI: "); qemu_debug_hex(r->esi); qemu_debug_string("  EDI: "); qemu_debug_hex(r->edi);
    qemu_debug_string("\n-- SEGMENT DUMP --\n");
    qemu_debug_string("CS:  "); qemu_debug_hex(r->cs);  qemu_debug_string("  DS:  "); qemu_debug_hex(r->ds);
    qemu_debug_string("  SS:  "); qemu_debug_hex(r->ss);
    qemu_debug_string("\n-- CONTROL DUMP --\n");
    qemu_debug_string("EIP: "); qemu_debug_hex(r->eip);
    qemu_debug_string("  EFLAGS: "); qemu_debug_hex(r->eflags);
    qemu_debug_string("\n-- STACK DUMP --\n");
    qemu_debug_string("ESP: "); qemu_debug_hex(r->esp);
    qemu_debug_string("  EBP: "); qemu_debug_hex(r->ebp);
    qemu_debug_string("  UESP: "); qemu_debug_hex(r->useresp);
    qemu_debug_string("\n");
}

// Dumps the state of all CPU registers from a cpu_state_t struct.
void qemu_debug_cpu_state(cpu_state_t *s) {
    qemu_debug_string("-- CPU STATE DUMP --\n");
    qemu_debug_string("EAX: "); qemu_debug_hex(s->eax); qemu_debug_string("  EBX: "); qemu_debug_hex(s->ebx);
    qemu_debug_string("  ECX: "); qemu_debug_hex(s->ecx); qemu_debug_string("\nEDX: "); qemu_debug_hex(s->edx);
    qemu_debug_string("  ESI: "); qemu_debug_hex(s->esi); qemu_debug_string("  EDI: "); qemu_debug_hex(s->edi);
    qemu_debug_string("\n-- SEGMENT DUMP --\n");
    qemu_debug_string("CS:  "); qemu_debug_hex(s->cs);  qemu_debug_string("  SS:  "); qemu_debug_hex(s->ss);
    qemu_debug_string("\n-- CONTROL DUMP --\n");
    qemu_debug_string("EIP: "); qemu_debug_hex(s->eip);
    qemu_debug_string("  EFLAGS: "); qemu_debug_hex(s->eflags);
    qemu_debug_string("\n-- STACK DUMP --\n");
    qemu_debug_string("ESP: "); qemu_debug_hex(s->esp);
    qemu_debug_string("  EBP: "); qemu_debug_hex(s->ebp);
    qemu_debug_string("\n-- PAGING DUMP --\n");
    qemu_debug_string("CR3: "); qemu_debug_hex(s->cr3);
    qemu_debug_string("\n");
}

// print an unsigned decimal integer to the debug log
void qemu_debug_dec(uint32_t n) {
    if (n == 0) {
        port_byte_out(0xE9, '0');
        return;
    }

    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    
    // The digits are in reverse order, so print them backwards.
    while (i > 0) {
        port_byte_out(0xE9, buf[--i]);
    }
}