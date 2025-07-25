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
#include <kernel/debug.h> // debug prints
#include <kernel/cpu/process.h> // process_init()

// Make the global flag visible to kmain
extern volatile int multitasking_enabled;

// Let kmain know about the new assembly function
extern void start_multitasking(cpu_state_t* cpu_state);

// Tell this file about the process table defined in process.c
extern task_struct_t process_table[MAX_PROCESSES];

// Helper for debug prints
static inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Our first simple kernel task
void task_a() {
    qemu_debug_string("task_a: entered.\n");
    while (1) {
        print_char('A');
        for (int i = 0; i < 10000000; i++) {} // Delay loop
    }
}

// Our second simple kernel task
void task_b() {
    qemu_debug_string("task_b: entered.\n");
    while (1) {
        print_char('B');
        for (int i = 0; i < 10000000; i++) {} // Delay loop
    }
}

void kmain() {
    qemu_debug_string("KERNEL:\nkmain_start ");

    // initiate dynamic mem allocation
    // Modules like TSS and FS depend on malloc() being ready
    init_memory(); 
    qemu_debug_string("mem_init ");

     // Install the kernel's GDT
    gdt_install();
    qemu_debug_string("gdt_inst ");

    // Install the TSS right after the GDT
    tss_install(); 
    qemu_debug_string("tss_inst ");

    // Initialize the process table BEFORE syscalls and interrupts
    process_init(); // Sets up task_a and task_b
    qemu_debug_string("proc_init ");

    // Syscall install after TSS
    // syscall_install();
    // qemu_debug_string("sysc_inst ");

    // Remap the PIC first to get the hardware into a stable state.
    pic_remap(0x20, 0x28); // Master PIC at 0x20, Slave at 0x28
    qemu_debug_string("pic_remap ");

    // Now, install our Interrupt Service Routines and load the IDT.
    idt_install();
    qemu_debug_string("idt_inst ");

    // Install the keyboard driver.
    // keyboard_install();
    // qemu_debug_string("keyb_inst ");

    // Install the timer driver.
    timer_install(); // Install our new timer driver
    qemu_debug_string("pit_inst ");

    // clear the bios text
    sleep(800); // Pause for 2 seconds
    clear_screen();
    qemu_debug_string("clear_bios_scr ");

    // print the boot screen
    print_bootscreen();
    sleep(1600); // Pause for 2 seconds
    qemu_debug_string("print_boot_scr ");
    clear_screen();

    // Initialize the filesystem driver. This must be done after memory
    // is initialized, as it uses malloc().
    // init_fs();
    // qemu_debug_string("fs_init ");

    // Initialize the shell
    // shell_init();
    // qemu_debug_string("shell_init ");

    // Enable interrupts! From this point on, the CPU will respond to hardware.
    __asm__ __volatile__ ("sti");
    qemu_debug_string("interrupt_enable ");

    // Start the shell's main processing loop.
    // This function will only return if a program is launched.
    // shell_run();
    // qemu_debug_string("shell_proc_loop ");

    // Open the gate for the scheduler right before starting.
    multitasking_enabled = 1;
    qemu_debug_string("multitasking_enabled ");

    // Start multitasking by jumping to the first task
    qemu_debug_string("kmain: Calling start_multitasking...\n");
    start_multitasking(&process_table[0].cpu_state);
    qemu_debug_string("kmain: Returned from start_multitasking (this is an error).\n");

    // If shell_run returns, a program is running.
    // The kernel should now enter an idle state.
    while (1) {
        __asm__ __volatile__("hlt");
    }
}