// myos/kernel/timer.h

#ifndef TIMER_H
#define TIMER_H

#include "types.h"

// Initializes the PIT and registers its IRQ handler.
void timer_install();
uint32_t timer_get_ticks(); // getter func

#endif