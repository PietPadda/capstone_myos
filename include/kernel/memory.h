// myos/include/kernel/memory.h

#ifndef MEMORY_H
#define MEMORY_H

#include <kernel/types.h>

void init_memory();
void* malloc(uint32_t size);
void free(void* ptr);

#endif