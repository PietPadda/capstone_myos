// myos/kernel/irq.h

#ifndef IRQ_H
#define IRQ_H

#include <kernel/types.h>
#include <kernel/exceptions.h> // For registers_t struct

void irq_install_handler(int irq, void (*handler)(registers_t *r));
void irq_uninstall_handler(int irq);
void irq_install();
void irq_handler(registers_t *r);

#endif