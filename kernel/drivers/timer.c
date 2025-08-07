// myos/kernel/drivers/timer.c

#include <kernel/timer.h>
#include <kernel/irq.h>
#include <kernel/io.h>
#include <kernel/vga.h>         // For printing output
#include <kernel/cpu/process.h> // schedule()
#include <kernel/debug.h>       // For debug printing

// Make the global flag visible to this file
extern volatile int multitasking_enabled;

// Make the globally defined current_task pointer visible to this file.
extern task_struct_t* current_task;

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
        // qemu_debug_string("timer_handler: Fired. Calling task_switch...\n");
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

// This is a simple, blocking delay function for use when the scheduler is not active.
void delay_ms(uint32_t milliseconds) {
    uint32_t start_tick = timer_get_ticks();
    uint32_t ticks_to_wait = milliseconds / 10;
    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }
    uint32_t end_tick = start_tick + ticks_to_wait;

    while (timer_get_ticks() < end_tick) {
        // Re-enable interrupts and halt. This is safe for a single-threaded context.
        __asm__ __volatile__("sti\n\thlt");
    }
}

// Puts the current task to sleep for a specified number of milliseconds.
void sleep(uint32_t milliseconds) {
    uint32_t start_tick = timer_get_ticks();
    // Our timer is at 100Hz, so 1 tick happens every 10ms.
    uint32_t ticks_to_wait = milliseconds / 10;

    // If the duration is very short, still wait for at least one tick.
    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }

    // Set the state and wakeup time on the current task's PCB.
    current_task->state = TASK_STATE_SLEEPING;
    current_task->wakeup_time = start_tick + ticks_to_wait;

    qemu_debug_string("sleep: PID ");
    qemu_debug_hex(current_task->pid);
    qemu_debug_string(" sleeping until tick ");
    qemu_debug_hex(current_task->wakeup_time);
    qemu_debug_string(".\n");

    // Yield the CPU by halting. The scheduler will not run this task again
    // until the timer handler has woken it up.
    __asm__ __volatile__("hlt");
}

// PC SPEAKER CODE

// Plays a sound of a given frequency.
void play_sound(uint32_t frequency) {
    // A frequency of 0 means stop the sound.
    if (frequency == 0) {
        nosound();
        return;
    }

 	uint32_t divisor = 1193180 / frequency;

 	// Set the PIT to square wave mode (mode 3) on channel 2.
 	port_byte_out(0x43, 0xB6);
 	
    // Send the frequency divisor, low byte then high byte.
 	port_byte_out(0x42, (uint8_t)(divisor & 0xFF));
 	port_byte_out(0x42, (uint8_t)((divisor >> 8) & 0xFF));
 
 	// And finally, enable the speaker by setting bit 1 of port 0x61.
 	uint8_t tmp = port_byte_in(0x61);
  	if (tmp != (tmp | 3)) {
 		port_byte_out(0x61, tmp | 3);
 	}
}
 
// Turns the speaker off and resets the PIT channel to a known state.
void nosound() {
 	// Disconnect the speaker from the PIT output by clearing the relevant bits.
    uint8_t tmp = port_byte_in(0x61) & 0xFC;
 	port_byte_out(0x61, tmp);

    // Per best practices, reset the PIT channel to a known state.
    // We'll set it to a very high frequency (a small divisor) to ensure it's
    // quiet and in a predictable state for any other potential driver.
    uint16_t divisor = 1;
 	port_byte_out(0x43, 0xB6); // Select channel 2, square wave mode
 	port_byte_out(0x42, (uint8_t)(divisor & 0xFF));
 	port_byte_out(0x42, (uint8_t)((divisor >> 8) & 0xFF));
}
 
// A wrapper function to make a beep for a specific duration.
void beep(uint32_t frequency, uint32_t duration_ms) {
    play_sound(frequency);
    sleep(duration_ms); // Use the scheduler-aware sleep function.
    nosound();
}