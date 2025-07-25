// myos/kernel/cpu/irq.c

#include <kernel/irq.h>
#include <kernel/io.h>
#include <kernel/vga.h>

// Array of function pointers for handling custom IRQ handlers
static void *irq_routines[16] = {0};

// Install a custom handler for a given IRQ
void irq_install_handler(int irq, void (*handler)(registers_t *r)) {
    irq_routines[irq] = handler;
}

// Clear a handler for a given IRQ
void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

// The main IRQ handler, called from our assembly stub
void irq_handler(registers_t *r) {
    void (*handler)(registers_t *r);

    // Find the handler for this IRQ number
    handler = irq_routines[r->int_no - 32];
    if (handler) {
        handler(r);
    }

    // After a task switch, control never returns to this function.
    // For timer interrupts, the EOI is handled by either the timer_handler
    // itself (pre-multitasking) or the task_switch assembly (post-multitasking).
    // Therefore, we must NOT send an EOI for the timer here.
    if (r->int_no == 32) { // This is IRQ 0, the timer
        return; // The timer_handler/task_switch is responsible for the EOI.
    }

    // Send the EOI (End of Interrupt) signal to the PICs
    if (r->int_no >= 40) {
        port_byte_out(0xA0, 0x20); // Send EOI to slave PIC
    }
    port_byte_out(0x20, 0x20); // Send EOI to master PIC
}