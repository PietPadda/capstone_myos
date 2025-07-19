// myos/include/kernel/cpu/tss.h

#ifndef TSS_H
#define TSS_H

#include <kernel/types.h>

// This struct defines the layout of a Task State Segment.
struct tss_entry_struct {
   uint32_t prev_tss;   // The previous TSS - if we used hardware task switching
   uint32_t esp0;       // The kernel stack pointer to switch to when coming from user mode
   uint32_t ss0;        // The kernel stack segment to switch to
   uint32_t esp1;       // Unused...
   uint32_t ss1;
   uint32_t esp2;
   uint32_t ss2;
   uint32_t cr3;
   uint32_t eip;
   uint32_t eflags;
   uint32_t eax;
   uint32_t ecx;
   uint32_t edx;
   uint32_t ebx;
   uint32_t esp;
   uint32_t ebp;
   uint32_t esi;
   uint32_t edi;
   uint32_t es;         // The value to load into ES when switching to this task
   uint32_t cs;         // The value to load into CS when switching to this task
   uint32_t ss;         // The value to load into SS when switching to this task
   uint32_t ds;         // The value to load into DS when switching to this task
   uint32_t fs;         // The value to load into FS when switching to this task
   uint32_t gs;         // The value to load into GS when switching to this task
   uint32_t ldt;        // The LDT segment selector to use with this task
   uint16_t reserved1;
   uint16_t iomap_base; // The I/O map base address
} __attribute__((packed));

void tss_install();

#endif