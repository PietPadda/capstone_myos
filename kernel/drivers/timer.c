// myos/kernel/drivers/timer.c

#include <kernel/timer.h>
#include <kernel/irq.h>
#include <kernel/io.h>
#include <kernel/vga.h> // For printing output
#include <kernel/cpu/process.h> // schedule()

// Make the global flag visible to this file
extern volatile int multitasking_enabled;

static volatile uint32_t tick = 0;

// This new assembly function will perform the actual context switch.
// It is defined in the new switch.asm file.
extern void task_switch(registers_t* r);

// The handler that is called on every timer interrupt (IRQ 0).
static void timer_handler(registers_t *r) {
    tick++;
    // Only call the scheduler if multitasking has officially started!
    if (multitasking_enabled) {
        // The C handler's only job is to call the assembly switcher.
        task_switch(r); // On every timer tick, switch tasks!
    }
}

// Configures the PIT and installs the timer handler.
void timer_install() {
    // Install the handler for IRQ 0
    irq_install_handler(0, timer_handler);

    //  Configure the PIT
    uint32_t frequency = 100; // Target frequency in Hz
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte to the PIT command port.
    // Command 0x36 means: Channel 0, Access lo/hi, Mode 3 (square wave).
    port_byte_out(0x43, 0x36);

    // Send the frequency divisor, low byte then high byte.
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// tick getter func
uint32_t timer_get_ticks() {
    return tick;
}

// delay tick func
void sleep(uint32_t milliseconds) {
    uint32_t start_tick = timer_get_ticks();
    // Our timer is at 100Hz, so 1 tick happens every 10ms.
    uint32_t ticks_to_wait = milliseconds / 10;

    // If the duration is very short, still wait for at least one tick.
    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }

    uint32_t end_tick = start_tick + ticks_to_wait;

    while (timer_get_ticks() < end_tick) {
        // This is the fix: Re-enable interrupts and then immediately halt.
        // The CPU will wait here until the next timer interrupt arrives.
        __asm__ __volatile__("sti\n\thlt");
    }
}