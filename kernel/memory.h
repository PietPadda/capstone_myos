// myos/kernel/memory.h

#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

void init_memory();
void* malloc(uint32_t size);
void free(void* ptr);

#endif