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

// Make the process table visible
extern task_struct_t process_table[MAX_PROCESSES];

static volatile uint32_t tick = 0;

// This new assembly function will perform the actual context switch.
// It is defined in the new switch.asm file.
extern void task_switch(registers_t* r);

// The handler that is called on every timer interrupt (IRQ 0).
static void timer_handler(registers_t *r) {
    tick++;

    // Check for any tasks that need to be woken up.
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TASK_STATE_SLEEPING && tick >= process_table[i].wakeup_time) {
            // DEBUG: Print status of sleeping tasks once per second
            if (tick % 100 == 0) {
                qemu_debug_string("TIMER: Checking sleeping PID ");
                qemu_debug_dec(i);
                qemu_debug_string(". Tick=");
                qemu_debug_dec(tick);
                qemu_debug_string(", WakeupAt=");
                qemu_debug_dec(process_table[i].wakeup_time);
                qemu_debug_string("\n");
            }

            // The actual wake-up check
            if (tick >= process_table[i].wakeup_time) {
                qemu_debug_string("TIMER: Waking up PID ");
                qemu_debug_dec(i);
                qemu_debug_string(" at tick ");
                qemu_debug_dec(tick);
                qemu_debug_string("!\n");

                process_table[i].state = TASK_STATE_RUNNING;
            }
        }
    }

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

    // Atomically enable interrupts and then halt.
    // This allows the timer interrupt to wake the CPU from its halted state.
    __asm__ __volatile__("sti\n\thlt");
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
 
    // Unconditionally enable the speaker by setting bits 0 and 1 of port 0x61.
    // We still read the port first to avoid changing the other bits.
 	uint8_t tmp = port_byte_in(0x61);
    port_byte_out(0x61, tmp | 3);
}
 
// Turns the speaker off and resets the PIT channel to a known state.
void nosound() {
 	// Disconnect the speaker from the PIT output by clearing the relevant bits.
    uint8_t tmp = port_byte_in(0x61) & 0xFC;
 	port_byte_out(0x61, tmp);

    // Per best practices, reset the PIT channel to a known state.
    // Use a divisor of 0, which the PIT treats as 65536.
    // This sets the PIT to its lowest, most stable frequency (~18.2Hz),
    // preventing the high-frequency oscillation that was causing instability.
    uint16_t divisor = 0;
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

// A new, blocking beep function for debugging purposes.
// This function does NOT use the scheduler and will block all other tasks.
void beep_blocking(uint32_t frequency, uint32_t duration_ms) {
    play_sound(frequency);

    // Use a busy-wait loop instead of sleeping
    uint32_t start = timer_get_ticks();
    uint32_t ticks_to_wait = duration_ms / 10;
    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }
    
    while (timer_get_ticks() < start + ticks_to_wait) {
        // Do nothing, just spin.
    }

    nosound();
}