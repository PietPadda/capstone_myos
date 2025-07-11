// myos/kernel/timer.c

#include "timer.h"
#include "irq.h"
#include "io.h"
#include "vga.h" // For printing output

static uint32_t tick = 0;

// The handler that is called on every timer interrupt (IRQ 0).
static void timer_handler(registers_t *r) {
    tick++;
    
    // Every 100 ticks (approx. 1 second), print the uptime.
    if (tick % 100 == 0) {
        // This is not an ideal way to print, but it's great for a demo!
        // A better way would involve a separate kernel thread or task.
        print_string("\nSystem Uptime: ");
        print_hex(tick / 100);
        print_string(" seconds");
    }
}

// Configures the PIT and installs the timer handler.
void timer_install() {
    // 1. Install the handler for IRQ 0
    irq_install_handler(0, timer_handler);

    // 2. Configure the PIT
    uint32_t frequency = 100; // Target frequency in Hz
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte to the PIT command port.
    // Command 0x36 means: Channel 0, Access lo/hi, Mode 3 (square wave).
    port_byte_out(0x43, 0x36);

    // Send the frequency divisor, low byte then high byte.
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}