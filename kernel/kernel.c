// myos/kernel/kernel.c

// headers
#include <kernel/idt.h> // interrupt tables
#include <kernel/vga.h> // writing to screen
#include <kernel/io.h> // hardware comms
#include <kernel/keyboard.h> // Include our new keyboard driver header
#include <kernel/timer.h> // Include our new timer driver header
#include <kernel/shell.h> // shell for CLI
#include <kernel/memory.h> // heap memory allocation
#include <kernel/gdt.h> // kernel permanent GDT
#include <kernel/fs.h> // Filesystem driver for init_fs()
#include <kernel/cpu/tss.h> // Task-State Segment
#include <kernel/syscall.h> // User Mode Syscalls

// Helper for debug prints
static inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

void kmain() {
    outb(0xE9, 'S'); // Start of kmain

    // initiate dynamic mem allocation
    // Modules like TSS and FS depend on malloc() being ready
    init_memory(); 
    outb(0xE9, 'M'); // Memory initialized

     // Install the kernel's GDT
    gdt_install();
    outb(0xE9, 'G'); // GDT installed

    // Install the TSS right after the GDT
    tss_install(); 
    outb(0xE9, 't'); // TSS installed

    // Syscall install after TSS
    syscall_install();
    outb(0xE9, 's'); // Syscall installed

    // Remap the PIC first to get the hardware into a stable state.
    pic_remap(0x20, 0x28); // Master PIC at 0x20, Slave at 0x28
    outb(0xE9, 'P'); // PIC remapped

    // Now, install our Interrupt Service Routines and load the IDT.
    idt_install();
    outb(0xE9, 'I'); // IDT installed

    // Install the keyboard driver.
    keyboard_install();
    outb(0xE9, 'K'); // Keyboard installed

    // Install the timer driver.
    timer_install(); // Install our new timer driver
    outb(0xE9, 'T'); // Timer installed

    // clear the bios text
    clear_screen();

    // Initialize the filesystem driver. This must be done after memory
    // is initialized, as it uses malloc().
    init_fs();
    outb(0xE9, 'F'); // Filesystem Initialized

    // Initialize the shell
    shell_init();
    outb(0xE9, 's'); // Shell initialized

    // Enable interrupts! From this point on, the CPU will respond to hardware.
    __asm__ __volatile__ ("sti");
    outb(0xE9, '!'); // Interrupts enabled

    // Hang the CPU. The keyboard handler will run when keys are pressed.
    // Use hlt to efficiently idle the CPU until an interrupt occurs.
    while (1) {
        __asm__ __volatile__("hlt");
    }
}