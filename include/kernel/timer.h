// myos/include/kernel/timer.h

#ifndef TIMER_H
#define TIMER_H

#include <kernel/types.h>

// Initializes the PIT and registers its IRQ handler.
void timer_install();
uint32_t timer_get_ticks(); // getter func
void sleep(uint32_t ms);
void delay_ms(uint32_t ms);

#endif